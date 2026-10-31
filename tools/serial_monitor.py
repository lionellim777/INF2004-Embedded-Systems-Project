#!/usr/bin/env python3
"""Read-only Pico USB serial monitor; reconnects after flashing and saves a log."""
import argparse
from datetime import datetime
from pathlib import Path
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    raise SystemExit("pyserial is required: python3 -m pip install pyserial")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Optional fixed serial device; default: detect Pico VID")
    parser.add_argument("--log", type=Path)
    parser.add_argument("--seconds", type=float, help="Optional bounded capture duration")
    args = parser.parse_args()
    log_path = args.log or (Path(__file__).resolve().parents[1] / "serial-output" /
                           datetime.now().strftime("encoder-%Y%m%d-%H%M%S.log"))
    log_path.parent.mkdir(parents=True, exist_ok=True)
    deadline = time.monotonic() + args.seconds if args.seconds else float("inf")
    with log_path.open("a", buffering=1) as log:
        def say(message):
            line = f"[HOST {datetime.now():%H:%M:%S}] {message}"
            print(line, flush=True)
            log.write(line + "\n")

        say("AAI10 serial monitor: 115200 baud; no serial commands sent. Ctrl+C exits.")
        say(f"Log: {log_path}")
        waiting = False
        try:
            while time.monotonic() < deadline:
                ports = sorted(p.device for p in list_ports.comports() if p.vid == 0x2e8a)
                port = args.port or (ports[0] if len(ports) == 1 else None)
                if not port:
                    if not waiting:
                        say("Waiting for Pico USB serial. Flash UF2, then it should reconnect automatically.")
                        if len(ports) > 1:
                            say(f"Multiple Picos found: {ports}; use --port to select one.")
                        waiting = True
                    time.sleep(0.5)
                    continue
                try:
                    with serial.Serial(port, 115200, timeout=0.5, write_timeout=0.5) as device:
                        device.dtr = True
                        say(f"Connected: {port}. Firmware output follows:")
                        waiting = False
                        pending = bytearray()
                        while time.monotonic() < deadline:
                            data = device.read(device.in_waiting or 1)
                            if data:
                                pending.extend(data)
                                while b"\n" in pending:
                                    line, _, rest = pending.partition(b"\n")
                                    pending = bytearray(rest)
                                    text = line.rstrip(b"\r").decode("utf-8", errors="replace")
                                    print(text, flush=True)
                                    log.write(text + "\n")
                                if len(pending) > 16384:
                                    say("Oversized unterminated line; flushing received bytes.")
                                    text = pending.decode("utf-8", errors="replace")
                                    print(text, flush=True)
                                    log.write(text + "\n")
                                    pending.clear()
                except (serial.SerialException, OSError) as error:
                    say(f"Serial unavailable: {error}; retrying.")
                    time.sleep(1)
        except KeyboardInterrupt:
            say("Monitor stopped. No firmware changed.")


if __name__ == "__main__":
    main()
