#!/usr/bin/env python3
"""Host regression/sanitizer tests and Cortex-M0+ type-size check. No flashing."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware/app/robot_firmware"
TESTS = ROOT / "firmware/tests/barcode_decoder_test"


def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def main():
    compiler = shutil.which("clang")
    if compiler is None:
        raise SystemExit("clang is required")
    flags = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-g",
             "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", APP]
    with tempfile.TemporaryDirectory(prefix="inf2004-barcode-") as temp:
        build = Path(temp)
        common = [APP / "barcode_decoder.c", APP / "barcode_reader.c"]
        regression = build / "regression"
        run(*flags, *common, APP / "navigation_commands.c",
            TESTS / "barcode_decoder_test.c", "-o", regression)
        run(regression)
        packed = build / "packed"
        run(*flags, *common,
            TESTS / "packed_storage_test.c", "-o", packed)
        run(packed)
        probe = build / "arm_layout.c"
        probe.write_text(
            '#include "barcode_reader.h"\n'
            '_Static_assert(sizeof(barcode_reader_t) == 88, "ARM reader size");\n'
            '_Static_assert(sizeof(barcode_runs_t) == 73, "ARM run size");\n')
        run(compiler, "--target=arm-none-eabi", "-mcpu=cortex-m0plus", "-mthumb",
            "-ffreestanding", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-I", APP, "-fsyntax-only", probe)
        # Decoder has no libc dependency; also compile real packed code for ARM.
        run(compiler, "--target=arm-none-eabi", "-mcpu=cortex-m0plus", "-mthumb",
            "-ffreestanding", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Os",
            "-I", APP, "-c", APP / "barcode_decoder.c", "-o", build / "decoder.o")
        print("PASS Cortex-M0+ layout: runs 116 -> 73 B; reader 132 -> 88 B "
              "(44 B saved); decoder cross-compiled. Full Pico link is a separate check.")


if __name__ == "__main__":
    main()
