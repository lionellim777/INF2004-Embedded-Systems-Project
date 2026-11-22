#!/usr/bin/env python3
"""Measure Cortex-M0+ object sizes of the application state types with the minimum-bit layout ON and OFF.

Compiles a probe with arm-none-eabi-gcc (never the laptop's sizeof), reads the symbol sizes with nm and
writes docs/testing/representation/<date>-minbit-layouts.json plus a Markdown table. No hardware needed.
Run: py -3 -I tools/measure_minbit_layouts.py
"""
import json, os, re, subprocess, sys, tempfile
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "firmware/app/robot_firmware"
APP = ROOT / "firmware/app/mission_rtos"
TOOLCHAIN = Path(os.environ.get("PICO_TOOLCHAIN_PATH", Path.home() / ".pico-sdk/toolchain/15_2_Rel1")) / "bin"
GCC = str(TOOLCHAIN / "arm-none-eabi-gcc")
NM = str(TOOLCHAIN / "arm-none-eabi-nm")

# (name, header include, type expression, extra defines)
TYPES = [
    ("obstacle_bypass_t", "obstacle_bypass.h", "obstacle_bypass_t", []),
    ("course_control_t", "course_control.h", "course_control_t", []),
    ("motion_control_t", "motion_control.h", "motion_control_t", []),
    ("barcode_progress_t", "barcode_drive.h", "barcode_progress_t", []),
    ("center_search_t", "center_search.h", "center_search_t", []),
    ("center_trim_t", "center_trim.h", "center_trim_t", []),
    ("hump_roll_t", "hump_roll.h", "hump_roll_t", []),
    ("tilt_alert_t", "tilt_alert.h", "tilt_alert_t", []),
    ("motion_class_state_t", "motion_class.h", "motion_class_state_t", []),
    ("terrain_status_t", "terrain.h", "terrain_status_t", []),
]


def measure(compact):
    sizes = {}
    with tempfile.TemporaryDirectory(prefix="minbit-") as name:
        folder = Path(name)
        lines = ['#include <stdbool.h>', '#include <stdint.h>', '#include <math.h>']
        for header in sorted({t[1] for t in TYPES}):
            lines.append('#include "%s"' % header)
        for sym, _, expr, _ in TYPES:
            lines.append("const unsigned char size_%s[sizeof(%s)] = {0};" % (sym, expr))
        probe = folder / "probe.c"
        probe.write_text("\n".join(lines) + "\n")
        obj = folder / "probe.o"
        subprocess.run([GCC, "-mcpu=cortex-m0plus", "-mthumb", "-std=c11", "-Os", "-c", "-Wall", "-Wextra",
                        "-DROBOCAR_CENTER_LINE=1", "-DROBOCAR_COMPACT_CONTROL_STATE=%d" % compact,
                        "-DCENTER_BASELINE_COMPACT=%d" % compact, "-I", str(BASE), "-I", str(APP), str(probe),
                        "-o", str(obj)], check=True)
        out = subprocess.check_output([NM, "-S", "--size-sort", str(obj)], text=True)
        for line in out.splitlines():
            m = re.match(r"\S+ ([0-9a-f]+) \w size_(\w+)", line)
            if m:
                sizes[m.group(2)] = int(m.group(1), 16)
    return sizes


def main():
    before, after = measure(0), measure(1)
    rows = [dict(type=t[0], before=before[t[0]], after=after[t[0]], saved=before[t[0]] - after[t[0]]) for t in TYPES]
    report = dict(date=str(date.today()), target="Cortex-M0+ (arm-none-eabi-gcc -Os)", rows=rows,
                  note="Nested types are counted inside their containers; do not add the rows together.")
    out = ROOT / "docs/testing/representation" / ("%s-minbit-layouts.json" % date.today().strftime("%Y%m%d"))
    out.write_text(json.dumps(report, indent=2) + "\n")
    print("| Type | Before (B) | After (B) | Saved (B) |\n| --- | ---: | ---: | ---: |")
    for r in rows:
        print("| `%s` | %d | %d | %d |" % (r["type"], r["before"], r["after"], r["saved"]))
    print("wrote", out)


if __name__ == "__main__":
    sys.exit(main())
