#!/usr/bin/env python3
"""Tests for the barcode-action preset: payload packing, the car's report decoder and the dashboard defaults.

The firmware side (firmware/app/mission_rtos/barcode_actions.c) has no host test yet because this project's
C tests need a host compiler; the fixtures below are hand-packed from its documented format.

    py -3 tools/test_rtos_barcode_actions.py
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rtos_dashboard import ACTION_NAMES, DEFAULT_CODES, pack_actions  # noqa: E402
from rtos_mqtt_observer import decode  # noqa: E402

TOPIC = "inf2004/aai10/robot1/v1/barcode_actions"


class TrackStatus(unittest.TestCase):
    TOPIC = "inf2004/aai10/robot1/v1/track_status"

    def test_reason_is_decoded(self):
        record = decode(self.TOPIC, b"CENTER_LINE_NOT_REACQUIRED")
        self.assertEqual(record["status"], "CENTER_LINE_NOT_REACQUIRED")

    def test_bad_payloads_are_refused(self):
        for bad in (b"", b"x" * 49, b"bad\x00byte", b"caf\xc3\xa9"):
            with self.assertRaises(ValueError):
                decode(self.TOPIC, bad)


class BarcodeActions(unittest.TestCase):
    def test_default_payload_is_hand_packed(self):
        # A=5 B=6 -> 0x65, C=1 D=4 -> 0x41, E..X zero, Y=0 Z=1 -> 0x10 (Z is the high nibble of byte 12).
        expected = bytes([0x65, 0x41] + [0] * 10 + [0x10])
        self.assertEqual(pack_actions(DEFAULT_CODES), expected)

    def test_every_action_round_trips_through_the_report(self):
        for code in range(len(ACTION_NAMES)):
            codes = [0] * 26
            codes[7] = code          # H: high nibble of byte 3
            codes[24] = code         # Y: low nibble of byte 12
            report = pack_actions(codes) + bytes([1])
            record = decode(TOPIC, report)
            self.assertEqual(record["codes"], codes)
            self.assertEqual(record["result"], 1)

    def test_rejects_bad_codes(self):
        for bad in ([0] * 25, [0] * 27, [12] + [0] * 25, [-1] + [0] * 25, [True] + [0] * 25, [1.0] + [0] * 25):
            with self.assertRaises(ValueError):
                pack_actions(bad)

    def test_report_decoder_rejects_bad_records(self):
        with self.assertRaises(ValueError):
            decode(TOPIC, bytes(13))                       # too short
        with self.assertRaises(ValueError):
            decode(TOPIC, bytes([0xff] * 13) + bytes([1]))  # nibble 15 means "unchanged": never reported
        with self.assertRaises(ValueError):
            decode(TOPIC, bytes(13) + bytes([4]))           # unknown result code

    def test_names_cover_every_code(self):
        self.assertEqual(len(ACTION_NAMES), 12)
        self.assertEqual(len(DEFAULT_CODES), 26)


if __name__ == "__main__":
    unittest.main()
