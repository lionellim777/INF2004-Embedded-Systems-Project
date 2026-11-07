#!/usr/bin/env python3
"""Decode motion/v1 binary records or [MOTION_V1] USB log lines. No network I/O."""
import argparse
import json
import re
import sys


def decode(payload):
    if len(payload) != 13 or payload[12] & 0xfc:
        raise ValueError("Expected 13 bytes with zero tail padding")
    word = int.from_bytes(payload, "little")

    def field(offset, bits, signed=False):
        value = (word >> offset) & ((1 << bits) - 1)
        return value - (1 << bits) if signed and value & (1 << (bits-1)) else value

    result = dict(captured_ms=field(0, 32), left_counts=field(32, 16, True),
                  right_counts=field(48, 16, True), left_mm_s=field(64, 10, True),
                  right_mm_s=field(74, 10, True), kind=field(84, 3),
                  phase=field(87, 3), fault=field(90, 3), stop_reason=field(93, 3),
                  valid=bool(field(96, 1)), stop_latched=bool(field(97, 1)))
    if result["kind"] > 4 or result["phase"] > 4 or result["fault"] > 5 or result["stop_reason"] > 6:
        raise ValueError("Invalid state code")
    if abs(result["left_counts"]) > 30000 or abs(result["right_counts"]) > 30000 or \
            abs(result["left_mm_s"]) > 500 or abs(result["right_mm_s"]) > 500:
        raise ValueError("Numeric value outside motion/v1 range")
    if not result["valid"] and any(result[k] for k in ("left_counts", "right_counts", "left_mm_s", "right_mm_s")):
        raise ValueError("Invalid sample has nonzero numeric fields")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hex", help="One raw 26-character hex record")
    args = parser.parse_args()
    try:
        if args.hex is not None:
            print(json.dumps(decode(bytes.fromhex(args.hex))))
        else:
            for line in sys.stdin:
                match = re.search(r"\[MOTION_V1\] ([0-9a-fA-F]+)", line)
                if match:
                    print(json.dumps(decode(bytes.fromhex(match[1]))), flush=True)
    except ValueError as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
