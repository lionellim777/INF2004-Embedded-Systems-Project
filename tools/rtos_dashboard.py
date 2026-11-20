#!/usr/bin/env python3
"""Live dashboard for the RTOS car (no Python packages needed).

Subscribes with Mosquitto's `mosquitto_sub`, decodes every binary record with the project's decoder
(tools/rtos_mqtt_observer.py) and serves a web page (tools/rtos_dashboard.html, styled like the AAI10-Calibrate
dashboard) on this computer.

It can start, stop or steer nothing. The one thing it can send is the barcode-action preset (which action each
barcode letter triggers) through `mosquitto_pub`, to command/set_barcode_actions. The car accepts that only
while no run is armed, so it stays within the "no remote control" rule. Use --no-send to disable even that.

    py -3 -u tools/rtos_dashboard.py                              # broker on this computer, then open http://localhost:8080
    py -3 -u tools/rtos_dashboard.py --log build/rtos-network-local/run.jsonl
    py -3 -u tools/rtos_dashboard.py --replay build/rtos-network-local/run.jsonl --speed 20   # no car needed

Stop with Ctrl+C.
"""
import argparse
import json
import shutil
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rtos_mqtt_observer import decode  # noqa: E402

PREFIX = "inf2004/aai10/robot1/v1/"
MOTION_STATES = ["STATIONARY", "ACCELERATING", "CRUISING", "DECELERATING", "TURNING", "CLIMBING HUMP", "DESCENDING HUMP"]
STOP_REASONS = ["BOOT", "BUTTON", "SERVICE", "MOTION", "INIT", "RANGE", "COMPLETE"]
MAX_EVENTS = 300
# Barcode-action preset. Codes are the 4-bit nibbles of firmware/app/mission_rtos/barcode_actions.h.
ACTION_NAMES = ["None (record only)", "Straight", "Left 90° now", "Right 90° now", "U-turn now",
                "Left at next junction", "Right at next junction", "Skip 1 junction, then left",
                "Skip 1 junction, then right", "Skip 2 junctions, then left", "Skip 2 junctions, then right", "Stop"]
# Must match DEFAULT_TABLE in barcode_actions.c: A left at next junction, B right at next junction,
# C straight, D U-turn, Z straight, everything else none.
DEFAULT_CODES = [5, 6, 1, 4] + [0] * 21 + [1]
ACTIONS_COMMAND = PREFIX + "command/set_barcode_actions"
PAGE_FILE = Path(__file__).resolve().with_name("rtos_dashboard.html")

ARGS = argparse.Namespace(no_send=True, replay=None, host="localhost", port=1883, mosquitto_pub=None)
CAN_SEND = [False]
lock = threading.Lock()
latest = {}      # topic -> {"record": dict, "wall": time of arrival}
tasks = {}       # runtime_diag task name -> record
events = []      # newest last: {t, system, sev, text}
counts = {}
undecodable = 0
started = time.time()
run = {"active": False, "start": None, "end": None, "max_roll": 0.0, "reads": [], "prev_state": None,
       "state": None, "seen_motion": False}
last_link = None
last_barcode_ms = None
last_tilt_alerts = 0


def note(system, sev, text):
    events.append({"t": round(time.time() - started, 1), "system": system, "sev": sev, "text": text})
    del events[:-MAX_EVENTS]


def ingest(topic, record):
    """Store one decoded record and derive events and per-run values. Runs under `lock`."""
    global last_link, last_barcode_ms, last_tilt_alerts
    now = time.time()
    latest[topic] = {"record": record, "wall": now}
    counts[topic] = counts.get(topic, 0) + 1
    if topic == "runtime_diag" and record.get("record") == "task" and record.get("task"):
        tasks[record["task"]] = record
    elif topic == "link" and record.get("state") != last_link:
        last_link = record.get("state")
        note("Link", "ok" if last_link == "online" else "fault", "Wi-Fi/MQTT link " + str(last_link))
    elif topic == "barcode" and record.get("valid") and record.get("barcode") \
            and record.get("captured_ms") != last_barcode_ms:
        last_barcode_ms = record.get("captured_ms")
        run["reads"].append(record["barcode"])
        note("Barcode", "ok", "Barcode read: " + str(record["barcode"]))
    elif topic == "track_status":
        status = record.get("status")
        if status != run.get("track_status"):
            run["track_status"] = status
            # Only the exceptional states are worth an event; normal driving states repeat constantly.
            if run["active"] is False and status not in (None, "TRACK_STOPPED", "IDLE") \
                    or any(k in str(status) for k in ("NOT_REACQUIRED", "EXHAUSTED", "TIMEOUT", "WRONG", "AMBIGUOUS", "SKEW", "TOO_WIDE", "LIMIT", "FAULT", "STALE", "LATE")):
                note("Course", "warn", "Course status: " + str(status))
    elif topic == "hump_event":
        note("Terrain", "ok", "Hump #%d detected, height %.1f mm" % (record["index"], record["height_dmm"] / 10))
    elif topic == "terrain":
        alerts = record.get("tilt_alerts", 0)
        if alerts > last_tilt_alerts:
            note("Tilt", "warn", "Tilt alert #%d (stops the run if the car is armed: pick-up)" % alerts)
        last_tilt_alerts = alerts
        state = record.get("motion_state", 0)
        if state != run["state"]:
            run["prev_state"] = run["state"]
            run["state"] = state
        if run["active"]:
            run["max_roll"] = max(run["max_roll"], abs(record.get("roll_decideg", 0)) / 10)
    elif topic == "motion":
        latched = bool(record.get("stop_latched"))
        if run["seen_motion"] and not latched and not run["active"]:
            run.update(active=True, start=now, end=None, max_roll=0.0, reads=[])
            note("Run", "ok", "Run started")
        elif run["seen_motion"] and latched and run["active"]:
            run.update(active=False, end=now)
            reason = STOP_REASONS[min(record.get("stop_reason", 0), 6)]
            why = run.get("track_status")
            note("Run", "ok" if reason in ("COMPLETE", "BUTTON") else "warn",
                 "Run stopped, reason " + reason + (", course: " + why if why else ""))
        elif not run["seen_motion"] and not latched:
            run.update(active=True, start=now, end=None)
        run["seen_motion"] = True


def pack_actions(codes):
    """26 action codes (0..11) -> the 13-byte command payload, letter A in the low nibble of byte 0."""
    if len(codes) != 26 or any(type(c) is not int or not 0 <= c < len(ACTION_NAMES) for c in codes):
        raise ValueError("expected 26 action codes between 0 and %d" % (len(ACTION_NAMES) - 1))
    return bytes(codes[2 * i] | (codes[2 * i + 1] << 4) for i in range(13))


def send_actions(args, codes):
    """Publish the preset with mosquitto_pub. Returns (ok, message)."""
    if args.no_send or args.replay:
        return False, "Sending is disabled (--no-send or --replay)."
    exe = args.mosquitto_pub or shutil.which("mosquitto_pub") or r"C:\Program Files\mosquitto\mosquitto_pub.exe"
    if not Path(exe).exists() and not shutil.which(exe):
        return False, "mosquitto_pub not found. Install Mosquitto, or pass --mosquitto-pub <path>."
    try:
        payload = pack_actions(codes)
    except ValueError as error:
        return False, str(error)
    command = [exe, "-h", args.host, "-p", str(args.port), "-t", ACTIONS_COMMAND, "-q", "1", "-s"]
    try:
        done = subprocess.run(command, input=payload, capture_output=True, timeout=10)
    except subprocess.TimeoutExpired:
        return False, "mosquitto_pub timed out."
    if done.returncode != 0:
        return False, "mosquitto_pub failed: " + done.stderr.decode(errors="replace").strip()[:200]
    with lock:
        note("Preset", "ok", "Barcode actions sent to the car")
    return True, "Sent. The car's reply appears in the table once it answers."


def mosquitto_lines(host, port, exe):
    command = [exe, "-h", host, "-p", str(port), "-t", PREFIX + "#", "-F", "%t %x"]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        for line in process.stdout:
            yield line
    finally:
        process.terminate()


def feed_from_broker(args, log):
    global undecodable
    exe = args.mosquitto_sub or shutil.which("mosquitto_sub") or r"C:\Program Files\mosquitto\mosquitto_sub.exe"
    if not Path(exe).exists() and not shutil.which(exe):
        sys.exit("mosquitto_sub not found. Install Mosquitto, or pass --mosquitto-sub <path>.")
    for line in mosquitto_lines(args.host, args.port, exe):
        line = line.strip()
        topic, _, hexdata = line.partition(" ")
        if not topic.startswith(PREFIX):
            continue
        kind = topic[len(PREFIX):]
        try:
            record = decode(topic, bytes.fromhex(hexdata))
        except (ValueError, KeyError, IndexError):
            with lock:
                undecodable += 1
            continue
        with lock:
            ingest(kind, record)
        if log:
            log.write(json.dumps(dict(record, topic=kind, t=round(time.time() - started, 2))) + "\n")
            log.flush()


def feed_from_replay(path, speed):
    previous = None
    for line in open(path, encoding="utf-8"):
        try:
            record = json.loads(line)
        except ValueError:
            continue
        topic, stamp = record.pop("topic", None), record.pop("t", 0)
        if topic is None:
            continue
        if previous is not None and stamp > previous:
            time.sleep(min((stamp - previous) / speed, 0.5))
        previous = stamp
        with lock:
            ingest(topic, record)


def snapshot():
    now = time.time()
    with lock:
        topics = {k: dict(v["record"], age=round(now - v["wall"], 1)) for k, v in latest.items()}
        duration = None
        if run["start"] is not None:
            duration = round((now if run["active"] else run["end"]) - run["start"], 1)
        data = {
            "uptime": round(now - started, 1), "topics": topics, "tasks": list(tasks.values()),
            "events": events[-120:], "counts": counts, "undecodable": undecodable,
            "motion_states": MOTION_STATES, "stop_reasons": STOP_REASONS,
            "action_names": ACTION_NAMES, "default_codes": DEFAULT_CODES, "can_send": CAN_SEND[0],
            "run": {"active": run["active"], "duration": duration, "max_roll": round(run["max_roll"], 1),
                    "reads": run["reads"][-20:], "prev_state": run["prev_state"]},
        }
    return data


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):  # noqa: N802
        path = self.path.split("?")[0]
        if path == "/state":
            body, kind = json.dumps(snapshot()).encode(), "application/json"
        elif path in ("/", "/index.html"):
            body, kind = PAGE_FILE.read_bytes(), "text/html; charset=utf-8"
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):  # noqa: N802
        if self.path.split("?")[0] != "/barcode_actions":
            self.send_error(404)
            return
        # A browser only sends application/json cross-site after a preflight this server never answers,
        # so another web page cannot make this computer publish the preset.
        if self.headers.get("Content-Type", "").split(";")[0].strip() != "application/json":
            self.send_error(415)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 1024:
                raise ValueError
            codes = json.loads(self.rfile.read(length))["codes"]
            ok, message = send_actions(ARGS, codes)
        except (ValueError, KeyError, TypeError):
            ok, message = False, "Bad request."
        body = json.dumps({"ok": ok, "message": message}).encode()
        self.send_response(200 if ok else 400)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="localhost", help="broker address (default: this computer)")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--web-port", type=int, default=8080)
    parser.add_argument("--log", help="also write every decoded record to this JSON-lines file")
    parser.add_argument("--replay", help="replay a recorded JSON-lines run instead of connecting to the broker")
    parser.add_argument("--speed", type=float, default=1.0, help="replay speed multiplier")
    parser.add_argument("--mosquitto-sub", help="path to mosquitto_sub if it is not on PATH")
    parser.add_argument("--mosquitto-pub", help="path to mosquitto_pub if it is not on PATH")
    parser.add_argument("--no-send", action="store_true", help="never publish anything, not even the barcode-action preset")
    args = parser.parse_args()
    vars(ARGS).update(vars(args))
    CAN_SEND[0] = not (args.no_send or args.replay)

    if not PAGE_FILE.exists():
        sys.exit("Missing " + str(PAGE_FILE))
    log = open(args.log, "a", encoding="utf-8") if args.log and not args.replay else None
    if args.replay:
        worker = threading.Thread(target=feed_from_replay, args=(args.replay, args.speed), daemon=True)
    else:
        worker = threading.Thread(target=feed_from_broker, args=(args, log), daemon=True)
    worker.start()
    server = ThreadingHTTPServer(("127.0.0.1", args.web_port), Handler)  # this computer only
    print("Dashboard on http://localhost:%d  (Ctrl+C to stop)" % args.web_port, flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
