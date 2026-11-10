#!/usr/bin/env python3
"""Run actual motion runtime/PWM gate against mocked SDK, plus binary codec tests."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware/app/mission_rtos"
BASE = ROOT / "firmware/app/robot_firmware"
TEST = ROOT / "firmware/tests/mission_rtos"


def main():
    spec = importlib.util.spec_from_file_location("motion_decoder", ROOT / "tools/decode_motion_record.py")
    decoder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(decoder)
    with tempfile.TemporaryDirectory(prefix="rtos-motion-test-") as name:
        temp = Path(name)
        for path in ["hardware/clocks.h", "hardware/pwm.h", "hardware/sync.h", "pico/stdlib.h"]:
            target = temp / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('#include "test_sdk.h"\n')
        cc = shutil.which("clang")
        subprocess.run([cc, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-I", str(APP), "-I", str(BASE), "-I", str(TEST), "-I", str(temp),
                        str(APP / "board.c"), str(APP / "safety_gate.c"), str(APP / "motion_record.c"),
                        str(APP / "motion_runtime.c"), str(BASE / "motion_control.c"),
                        str(TEST / "foundation_test.c"), "-o", str(temp / "test")], check=True)
        output = subprocess.check_output([str(temp / "test")], text=True)
        print(output, end="")
        fixtures = [bytes.fromhex(line.split()[1]) for line in output.splitlines() if line.startswith("FIXTURE ")]
        assert len(fixtures) == 2
        zero, extreme = [decoder.decode(p) for p in fixtures]
        assert zero == dict(captured_ms=0, left_counts=0, right_counts=0, left_mm_s=0, right_mm_s=0,
                            kind=0, phase=0, fault=0, stop_reason=0, valid=True, stop_latched=False)
        assert extreme == dict(captured_ms=4294967295, left_counts=-30000, right_counts=30000,
                               left_mm_s=-500, right_mm_s=500, kind=4, phase=4, fault=5,
                               stop_reason=6, valid=True, stop_latched=True)
        for invalid in [b"", fixtures[0][:-1], fixtures[0] + b"\0", fixtures[0][:-1] + b"\x80"]:
            try:
                decoder.decode(invalid)
            except ValueError:
                pass
            else:
                raise AssertionError("Python decoder accepted malformed payload")
        toolchain = Path(os.environ.get("PICO_TOOLCHAIN_PATH", ROOT / "build/dependencies/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi"))
        gcc = toolchain / "bin/arm-none-eabi-gcc"
        probe = temp / "layout.c"
        probe.write_text('#include "motion_record.h"\n#include "safety_gate.h"\n#include "motion_runtime.h"\n'
                         'const unsigned sizes[] __attribute__((section(".sizes"))) = '
                         '{sizeof(motion_record_t),MOTION_RECORD_BYTES,sizeof(safety_gate_t),sizeof(motion_runtime_t)};\n')
        subprocess.run([str(gcc), "-mcpu=cortex-m0plus", "-mthumb", "-std=c11", "-I", str(APP),
                        "-I", str(BASE), "-c", str(probe), "-o", str(temp / "layout.o")], check=True)
        subprocess.run([str(gcc.with_name("arm-none-eabi-objcopy")), "-O", "binary", "-j", ".sizes",
                        str(temp / "layout.o"), str(temp / "layout.bin")], check=True)
        native, wire, gate, runtime = struct.unpack("<4I", (temp / "layout.bin").read_bytes())
        assert wire == 13 and gate == 8
        evidence = dict(native_equivalent_record_bytes=native, binary_record_bytes=wire,
                        safety_gate_bytes=gate, runtime_bytes=runtime, codec_cases=5005,
                        test_type="Host SDK simulation and target compile; no hardware/RTOS scheduling execution")
        out = ROOT / "build/rtos-motion-tests.json"
        out.parent.mkdir(exist_ok=True)
        out.write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"PASS independent Python decoder and ARM layout: native equivalent {native} B -> binary {wire} B")


if __name__ == "__main__":
    main()
