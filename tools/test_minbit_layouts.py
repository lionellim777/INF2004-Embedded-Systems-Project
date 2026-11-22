#!/usr/bin/env python3
"""Minimum-bit layout checks that need only a host C compiler (clang or gcc).

1. The obstacle-bypass controller, and the hump/tilt/motion-class state machines, are run on identical
   pseudo-random sensor streams with the packed layout OFF and ON; a hash of every output and state field
   must match. This proves the bit-field layout changes no behaviour.
2. The shared fault mask is exercised (independent bits, latched copy, every update interrupt-bracketed).
3. Every status string the firmware can report is in the binary track_status table, and the Python
   decoder reads the same table the firmware compiles.
Run: py -3 tools/test_minbit_layouts.py
"""
import os, re, shutil, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "firmware/app/robot_firmware"
APP = ROOT / "firmware/app/mission_rtos"
TESTS = ROOT / "firmware/tests"
CC = shutil.which("clang") or shutil.which("gcc")
if not CC:
    sys.exit("no host C compiler (clang or gcc) on PATH")


def build_and_run(folder, name, sources, compact, extra=()):
    exe = folder / ("%s_%s.exe" % (name, compact))
    subprocess.run([CC, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", "-DROBOCAR_CENTER_LINE=1",
                    "-DROBOCAR_COMPACT_CONTROL_STATE=%s" % compact, "-I" + str(BASE), *extra,
                    *map(str, sources), "-lm", "-o", str(exe)], check=True)
    return subprocess.check_output([str(exe)], text=True).strip()


def fields(line):
    return dict(p.split("=", 1) for p in line.split())


def main():
    with tempfile.TemporaryDirectory(prefix="minbit-test-") as name:
        folder = Path(name)
        for label, sources in (
                ("obstacle", [TESTS / "obstacle_layout_equivalence_test.c", BASE / "obstacle_bypass.c",
                              BASE / "motion_control.c", BASE / "course_control.c"]),
                ("observer", [TESTS / "observer_layout_equivalence_test.c"])):
            off, on = (fields(build_and_run(folder, label, sources, c)) for c in (0, 1))
            assert off["hash"] == on["hash"], (label, off, on)
            for key in off:
                if key not in ("hash", "sizeof"):
                    assert off[key] == on[key], (label, key)
            print("PASS %s: identical outputs and state over the same inputs (hash %s, sizeof %s -> %s)"
                  % (label, on["hash"], off["sizeof"], on["sizeof"]))
        # Shared fault mask with a fake hardware/sync.h.
        fake = folder / "fake/hardware"
        fake.mkdir(parents=True)
        (fake / "sync.h").write_text("#include <stdint.h>\nuint32_t save_and_disable_interrupts(void);"
                                     "void restore_interrupts(uint32_t);\n")
        exe = folder / "fault.exe"
        subprocess.run([CC, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(folder / "fake"), "-I" + str(APP),
                        str(TESTS / "fault_flags_test.c"), str(APP / "fault_flags.c"), "-o", str(exe)], check=True)
        print(subprocess.check_output([str(exe)], text=True).strip())
    # track_status table covers every status literal and matches the Python decoder.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import gen_track_status_codes as gen
    from rtos_mqtt_observer import TRACK_STATUS_NAMES
    assert tuple(gen.collect()) == TRACK_STATUS_NAMES, "track_status_codes.h is stale: run tools/gen_track_status_codes.py"
    assert 0 < len(TRACK_STATUS_NAMES) <= 255
    print("PASS track_status table: %d statuses, header equals the sources and the Python decoder" % len(TRACK_STATUS_NAMES))


if __name__ == "__main__":
    main()
