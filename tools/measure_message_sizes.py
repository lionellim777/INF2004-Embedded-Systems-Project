#!/usr/bin/env python3
"""Message-size comparison for the Week 13 evidence.

Builds one representative payload per MQTT topic with the REAL firmware encoders
(firmware/tests/message_size_probe.c), decodes it with the dashboard decoder, and compares the binary
payload with the same fields as compact JSON text (what a text protocol would send). Also compares the
old ASCII track_status string with the new 3-byte record. Writes docs/testing/representation/<date>-message-sizes.json.
Run: py -3 tools/measure_message_sizes.py   (needs clang or gcc)
"""
import json, shutil, subprocess, sys, tempfile
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware/app/mission_rtos"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from rtos_mqtt_observer import decode, TRACK_STATUS_NAMES  # noqa: E402

PREFIX = "inf2004/aai10/robot1/v1/"
CC = shutil.which("clang") or shutil.which("gcc")


def main():
    with tempfile.TemporaryDirectory(prefix="msg-") as name:
        exe = Path(name) / "probe.exe"
        subprocess.run([CC, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(APP),
                        str(ROOT / "firmware/tests/message_size_probe.c"), str(APP / "binary_protocol.c"),
                        str(APP / "motion_record.c"), "-o", str(exe)], check=True)
        lines = subprocess.check_output([str(exe)], text=True).split("\n")
    rows = []
    for line in filter(None, lines):
        topic, hexed = line.split()
        payload = bytes.fromhex(hexed)
        record = decode(PREFIX + topic, payload)
        text = json.dumps(record, separators=(",", ":"), sort_keys=True)
        rows.append(dict(topic=topic, binary_bytes=len(payload), json_bytes=len(text.encode()),
                         saving_percent=round(100 * (1 - len(payload) / len(text.encode())), 1)))
    statuses = [len(n) for n in TRACK_STATUS_NAMES]
    track = dict(old_ascii_bytes_min=min(statuses), old_ascii_bytes_mean=round(sum(statuses) / len(statuses), 1),
                 old_ascii_bytes_max=max(statuses), new_binary_bytes=3,
                 note="The new record also carries the active and latched fault masks, which the old text did not.")
    report = dict(date=str(date.today()), topics=rows, track_status_before_after=track,
                  total_binary=sum(r["binary_bytes"] for r in rows), total_json=sum(r["json_bytes"] for r in rows))
    out = ROOT / "docs/testing/representation" / ("%s-message-sizes.json" % date.today().strftime("%Y%m%d"))
    out.write_text(json.dumps(report, indent=2) + "\n")
    print("| Topic | Binary (B) | Same fields as JSON text (B) | Smaller by |\n| --- | ---: | ---: | ---: |")
    for r in rows:
        print("| %s | %d | %d | %s%% |" % (r["topic"], r["binary_bytes"], r["json_bytes"], r["saving_percent"]))
    print("| **all eight** | %d | %d | %s%% |" % (report["total_binary"], report["total_json"],
          round(100 * (1 - report["total_binary"] / report["total_json"]), 1)))
    print("track_status text before: %(old_ascii_bytes_min)d..%(old_ascii_bytes_max)d B (mean %(old_ascii_bytes_mean)s), now 3 B" % track)
    print("wrote", out)


if __name__ == "__main__":
    main()
