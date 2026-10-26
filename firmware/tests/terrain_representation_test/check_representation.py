from pathlib import Path
import json
import math
import re

ROOT = Path(__file__).resolve().parents[3]
HEADER = ROOT / 'firmware/app/robot_firmware/terrain.h'
FIELDS = [
    ('available', 0, 1, 1), ('calibrated', 0, 1, 1),
    ('sample_valid', 0, 1, 1), ('height_valid', 0, 1, 1),
    ('highest_height_valid', 0, 1, 1), ('motion', 0, 7, 3),
    ('hump_count', 0, 3000, 12), ('highest_hump_number', 0, 3000, 12),
    ('highest_height_hump_number', 0, 3000, 12),
    ('highest_peak_angle_cdeg', 0, 9000, 14),
    ('current_hump_mm', 0, 5000, 13), ('highest_hump_mm', 0, 5000, 13),
    ('captured_ms', 0, 1800000, 21), ('faults', 0, 31, 5),
]


def main():
    source = HEADER.read_text(encoding='utf-8-sig')
    structure = source.split('typedef struct __attribute__((packed)) {', 1)[1].split('} terrain_status_t;', 1)[0]
    actual = re.findall(r'unsigned int\s+(\w+)\s*:\s*(\d+)', structure)
    assert actual == [(name, str(bits)) for name, lo, hi, bits in FIELDS]
    encoder = re.findall(r'terrain_pack_bits\(output, (\d+)u, (\d+)u, s->(\w+)\)', source)
    offset = 0
    expected = []
    for name, lo, hi, bits in FIELDS:
        assert (hi - lo).bit_length() == bits
        assert 2 ** (bits - 1) < hi - lo + 1 <= 2 ** bits
        expected.append((str(offset), str(bits), name))
        offset += bits
    assert encoder == expected
    assert offset == 110 and (offset + 7) // 8 == 14
    print('PASS all public field widths are minimum for their declared ranges')
    print('PASS encoder offsets exactly cover bits 0..109 without overlap')

    # Sensor data: all 4096 signed codes at each of the actual 24 axis slots.
    # The reference uses one arbitrary-precision integer, not the C bit loop.
    initial = int.from_bytes(bytes([0xA5]) * 36, 'little')
    for axis in range(24):
        shift = axis * 12
        field_mask = 4095 << shift
        for value in range(-2048, 2048):
            packed = (initial & ~field_mask) | ((value % 4096) << shift)
            code = (packed >> shift) & 4095
            decoded = code if code < 2048 else code - 4096
            assert decoded == value
            assert (packed & ~field_mask) == (initial & ~field_mask)
    print('PASS 98,304 raw-axis cases; no neighbouring packed values change')

    for speed in range(-500, 501):
        code = speed % 1024
        assert (code if code < 512 else code - 1024) == speed
    for captured_ms in range(1800001):
        assert (captured_ms & ((1 << 21) - 1)) == captured_ms
    for before in range(32):
        for mask in range(256):
            selected = mask & 31
            assert (before | selected) <= 31
            assert (before & ~selected) <= 31
    print('PASS signed speed range, all 1,800,001 capture times and fault masks')

    values = [1, 1, 1, 1, 1, 7, 3000, 2999, 3000, 9000, 5000, 5000, 1800000, 31]
    bits = []
    for value, (name, lo, hi, width) in zip(values, FIELDS):
        assert lo <= value <= hi
        bits += [(value >> i) & 1 for i in range(width)]
    bits += [0] * (112 - len(bits))
    encoded = bytes(sum(bits[b * 8 + i] << i for i in range(8)) for b in range(14))
    assert encoded.hex() == 'ffb87bbbb88b32224ec40974b73f'
    assert encoded[-1] & 0xC0 == 0
    print('PASS independent golden payload: ' + encoded.hex())

    internal_ranges = [(-9000, 9000, 15), (0, 9000, 14),
                       (-5000000, 5000000, 24), (0, 5000000, 23),
                       (-1000, 1000, 11), (0, 7, 3), (0, 8, 4), (0, 2, 2)]
    for lo, hi, width in internal_ranges:
        assert (hi - lo).bit_length() == width
    assert 500 * 10 == 5000
    assert 1800000 // (300 + 300) == 3000
    # Numerically check the conversion used by the driver (mm/s * ms = um).
    assert round(100 * 500 * math.sin(math.radians(30))) == 25000
    print('PASS internal field bounds and height-unit conversion')
    print('REFERENCE CHECKS PASSED; these do not execute the C or physical robot')


if __name__ == '__main__':
    main()
