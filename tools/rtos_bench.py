#!/usr/bin/env python3
"""Guided USB bench logger. Selects bounded tests; physical GP20 alone starts."""
import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import select
import sys
import time
from zoneinfo import ZoneInfo

from decode_motion_record import decode

ROOT = Path(__file__).resolve().parents[1]
PHASES = ["idle", "active", "completed", "aborted", "fault"]
REASONS = ["boot/none", "user stop", "motor service deadline", "motion fault", "initialization fault", "data range", "completed"]


class Capture:
    def __init__(self, path):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.raw = self.path.with_suffix(".log").open("x", buffering=1)
        self.ready = False
        self.latest = None
        self.summary = dict(decoded_packets=0, parse_errors=0, max_motion_gap_us=0,
                            phases_seen=[], fault_codes_seen=[], stop_traces=[], commands_requested=[])

    def line(self, line):
        self.raw.write(line + "\n")
        info = re.search(r"\[RTOS_INFO\] version=(\S+)", line)
        if info:
            self.ready = info[1] in ("0.2", "0.3", "0.4")
            self.summary["firmware_version"] = info[1]
        match = re.search(r"\[MOTION_V1\] ([0-9a-fA-F]+) gap_us=(\d+)", line)
        if match:
            try:
                sample = decode(bytes.fromhex(match[1]))
            except ValueError:
                self.summary["parse_errors"] += 1
                return "Rejected a malformed motion record"
            changed = self.latest is None or any(self.latest[k] != sample[k] for k in ("phase", "fault", "stop_latched", "kind"))
            self.latest = sample
            self.summary["decoded_packets"] += 1
            self.summary["max_motion_gap_us"] = max(self.summary["max_motion_gap_us"], int(match[2]))
            self.summary["last_motion"] = sample
            for key, value in [("phases_seen", sample["phase"]), ("fault_codes_seen", sample["fault"])]:
                if value not in self.summary[key]: self.summary[key].append(value)
            if changed:
                return f"Controller: {PHASES[sample['phase']]} | stop latched: {sample['stop_latched']} | fault code: {sample['fault']}"
        trace = re.search(r"\[STOP_TRACE\] seq=(\d+) reason=(\d+) request_us=(\d+) off_us=(\d+) alarm_gap_us=(\d+)", line)
        if trace:
            sequence, reason, requested, disabled, gap = map(int, trace.groups())
            value = dict(sequence=sequence, reason=reason, requested_us=requested, disabled_us=disabled,
                         register_write_delay_us=(disabled-requested)&0xffffffff, max_alarm_gap_us=gap)
            self.summary["stop_traces"].append(value)
            label = REASONS[reason] if reason < len(REASONS) else "unknown"
            return f"Stop recorded: {label}. Detected request to PWM-off write: {value['register_write_delay_us']} us."
        if line.startswith("SELECT="):
            return "Test selected. Press and release GP20 on the car to start."
        if line.startswith("[RTOS_BOOT]"):
            return line
        if line.startswith('[TRACK_INFO]'):
            self.summary['last_track_info']=line
            if 'status=TRACK_START_NOT_ALIGNED' in line:
                if not self.summary.get('track_start_rejected'):
                    self.summary['track_start_rejected']=True
                    return 'Track start rejected: hold GP28 over black tape for one second, then press/release GP20 again.'
            else:self.summary['track_start_rejected']=False
        if line.startswith('[BARCODE_READ]'):
            self.summary.setdefault('barcode_reads',[]).append(line)
            return line
        return None

    def command(self, text):
        key = text.strip().lower()
        if key not in ("f", "l", "r", "u", "t", "x", "?"):
            raise ValueError("Use f, l, r, u, x or q")
        if not self.ready:
            raise ValueError("Waiting for supported RTOS identity; no commands sent")
        if key=='t' and self.summary.get('firmware_version')!='0.4':
            raise ValueError('Track mode requires the configured v0.4 firmware')
        if key in "flrut" and self.latest and not self.latest["stop_latched"]:
            raise ValueError("Stop the current test before choosing another")
        self.summary["commands_requested"].append(key)
        self.raw.write(f"[HOST_COMMAND] {key}\n")
        return key.encode("ascii")

    def close(self):
        self.raw.close()
        self.path.with_suffix(".json").write_text(json.dumps(self.summary, indent=2) + "\n")


def main():
    import serial
    from serial.tools import list_ports
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    stamp = datetime.now(ZoneInfo("Asia/Singapore")).strftime("%Y%m%d-%H%M%S")
    capture = Capture(args.log or ROOT / "serial-output" / f"rtos-bench-{stamp}")
    print("RTOS motor test — support the chassis so BOTH powered wheels can spin freely.")
    print("This checks motors and stopping; no hump is needed.")
    print("f = select 30cm | l/r/u = select turns | x = stop | q = save and exit")
    print("Selection never starts movement. Start with GP20 on the car.")
    print("Log:", capture.path.with_suffix(".log"))
    device = None
    waiting = False
    try:
        while True:
            if device is None:
                ports = sorted(p.device for p in list_ports.comports() if p.vid == 0x2e8a)
                port = args.port or (ports[0] if len(ports) == 1 else None)
                if not port:
                    if not waiting:
                        print("Waiting for the Pico's USB connection...", flush=True); waiting = True
                else:
                    candidate = None
                    try:
                        candidate = serial.Serial(port, 115200, timeout=0.05, write_timeout=0.5)
                        candidate.dtr = True
                        device = candidate; pending = bytearray(); waiting = False
                        print("Connected. Waiting for RTOS identity...", flush=True)
                    except (serial.SerialException, OSError):
                        if candidate is not None: candidate.close()
                        device = None
                        time.sleep(0.2)
            if device is not None:
                try:
                    chunk = device.read(min(device.in_waiting or 1, 4096))
                    pending.extend(chunk)
                    if len(pending) > 16384:
                        raise ValueError("Oversized unterminated USB line")
                    while b"\n" in pending:
                        line, _, rest = pending.partition(b"\n"); pending = bytearray(rest)
                        was_ready = capture.ready
                        message = capture.line(line.rstrip(b"\r").decode("utf-8", errors="replace"))
                        if capture.ready and not was_ready:
                            print(f"RTOS v{capture.summary['firmware_version']} confirmed. Wheels should still be stopped. Type f, then Enter.", flush=True)
                        if message: print(message, flush=True)
                except (serial.SerialException, OSError, ValueError) as error:
                    print(f"USB disconnected: {error}. Use GP20 to stop if needed.", flush=True)
                    device.close(); device = None; capture.ready = False
            if select.select([sys.stdin], [], [], 0.05)[0]:
                text = sys.stdin.readline()
                if not text or text.strip().lower() == "q": break
                try:
                    if device is None: raise ValueError("Pico is disconnected")
                    payload = capture.command(text)
                    device.write(payload)
                except (ValueError, serial.SerialException) as error:
                    print(error, flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        stop_sent = False
        if device is not None:
            if capture.ready:
                try:
                    stop_sent = device.write(capture.command("x")) == 1
                    device.flush()
                except (OSError, serial.SerialException): pass
            device.close()
        capture.close()
        print("Saved:", capture.path.with_suffix(".log"))
        print("Saved:", capture.path.with_suffix(".json"))
        print("STOP sent. Confirm the wheels are stopped before lowering the car." if stop_sent else
              "No USB stop could be sent. Use GP20 if needed; confirm the wheels are stopped before lowering the car.")


if __name__ == "__main__": main()
