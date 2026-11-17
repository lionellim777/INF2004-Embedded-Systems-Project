#!/usr/bin/env python3
"""Windows-friendly MQTT listener for the RTOS car (no Python packages needed).

Subscribes with Mosquitto's own `mosquitto_sub`, decodes every binary record with the project's decoder
(tools/rtos_mqtt_observer.py) and prints one readable line per record. It never publishes anything.

    python tools/rtos_windows_listen.py                    # broker on this computer, port 1883
    python tools/rtos_windows_listen.py --host 192.168.1.20 --log build/rtos-network-local/run.jsonl
    mosquitto_sub -t "inf2004/aai10/robot1/v1/#" -F "%t %x" | python tools/rtos_windows_listen.py --stdin

Stop with Ctrl+C: a summary of the records seen is printed.
"""
import argparse
import json
import shutil
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rtos_mqtt_observer import decode  # noqa: E402

PREFIX = "inf2004/aai10/robot1/v1/"
STATES = ["STATIONARY", "ACCELERATING", "CRUISING", "DECELERATING", "TURNING", "CLIMBING_HUMP", "DESCENDING_HUMP"]


def describe(record):
    """One short human line for the records that matter for terrain; others show their raw fields."""
    kind = record.get("type")
    if kind == "terrain":
        return ("valid=%d cal=%d roll=%.1f deg humps=%d last=%.1f mm highest=%.1f mm motion=%s tilt=%d alerts=%d" % (
            record["valid"], record["calibrated"], record["roll_decideg"] / 10, record["humps"],
            record["last_height_dmm"] / 10, record["highest_height_dmm"] / 10,
            STATES[record["motion_state"]], record["tilt_active"], record["tilt_alerts"]))
    if kind == "hump_event":
        return "HUMP #%d height=%.1f mm dropped=%d" % (record["index"], record["height_dmm"] / 10, record["dropped"])
    return " ".join("%s=%s" % (k, v) for k, v in record.items() if k != "type")


def lines_from_mosquitto(host, port, exe):
    command = [exe, "-h", host, "-p", str(port), "-t", PREFIX + "#", "-F", "%t %x"]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        for line in process.stdout:
            yield line
    finally:
        process.terminate()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="localhost", help="broker address (default: this computer)")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--log", help="also write every decoded record to this JSON-lines file")
    parser.add_argument("--stdin", action="store_true", help="read 'topic hexpayload' lines from stdin instead")
    parser.add_argument("--mosquitto-sub", help="path to mosquitto_sub if it is not on PATH")
    args = parser.parse_args()

    if args.stdin:
        source = sys.stdin
    else:
        exe = args.mosquitto_sub or shutil.which("mosquitto_sub") or r"C:\Program Files\mosquitto\mosquitto_sub.exe"
        if not Path(exe).exists() and not shutil.which(exe):
            sys.exit("mosquitto_sub not found. Install Mosquitto, or pass --mosquitto-sub <path>.")
        source = lines_from_mosquitto(args.host, args.port, exe)

    log = open(args.log, "a", encoding="utf-8") if args.log else None
    counts, errors, humps = Counter(), Counter(), []
    start = time.monotonic()
    try:
        for line in source:
            line = line.strip()
            if not line:
                continue
            topic, _, hexdata = line.partition(" ")
            if not topic.startswith(PREFIX):
                print("%7.1f  (broker) %s" % (time.monotonic() - start, line))
                continue
            kind = topic[len(PREFIX):]
            try:
                record = decode(topic, bytes.fromhex(hexdata))
            except (ValueError, KeyError, IndexError) as error:
                errors[kind] += 1
                print("%7.1f  %-10s  UNDECODABLE: %s (%s)" % (time.monotonic() - start, kind, error, hexdata))
                continue
            counts[kind] += 1
            if kind == "hump_event":
                humps.append(record)
            print("%7.1f  %-10s  %s" % (time.monotonic() - start, kind, describe(record)), flush=True)
            if log:
                log.write(json.dumps(dict(record, topic=kind, t=round(time.monotonic() - start, 2))) + "\n")
                log.flush()
    except KeyboardInterrupt:
        pass
    print("\nSummary after %.0f s: %s" % (time.monotonic() - start, dict(counts) or "no records"))
    if errors:
        print("Undecodable records:", dict(errors))
    if humps:
        print("Hump events:", [(h["index"], h["height_dmm"] / 10) for h in humps])


if __name__ == "__main__":
    main()
