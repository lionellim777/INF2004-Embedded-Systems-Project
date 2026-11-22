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
        from rtos_mqtt_observer import TRACK_STATUS_NAMES
        code = TRACK_STATUS_NAMES.index("CENTER_LINE_NOT_REACQUIRED") + 1
        record = decode(self.TOPIC, bytes([code, 0b000101, 0b010101]))
        self.assertEqual(record["status"], "CENTER_LINE_NOT_REACQUIRED")
        self.assertEqual(record["faults"], ["MOTOR_STALL", "IMU"])
        self.assertEqual(record["faults_latched"], ["MOTOR_STALL", "IMU", "WIFI_LOSS"])

    def test_unknown_code_zero_is_reported(self):
        self.assertEqual(decode(self.TOPIC, bytes(3))["status"], "UNKNOWN")

    def test_bad_payloads_are_refused(self):
        from rtos_mqtt_observer import TRACK_STATUS_NAMES
        for bad in (b"", bytes([1, 0]), bytes([1, 0, 0, 0]), b"CENTER_LINE_NOT_REACQUIRED",
                    bytes([1, 0x40, 0]), bytes([1, 0, 0x80]), bytes([len(TRACK_STATUS_NAMES) + 1, 0, 0])):
            with self.assertRaises(ValueError):
                decode(self.TOPIC, bad)


def obstacle_payload(phase, servo=90, range_mm=None, detected=0, completed=0, active=True):
    """Same bit layout as obstacle_record_encode in binary_protocol.c."""
    fields = 0
    if active:
        fields = 1 | phase << 1 | servo << 6 | detected << 26 | completed << 34
        if range_mm is not None:
            fields |= 1 << 13 | range_mm << 14
    return (1000).to_bytes(4, "little") + fields.to_bytes(6, "little")


class ObstacleBoard(unittest.TestCase):
    TOPIC = "inf2004/aai10/robot1/v1/obstacle"

    def setUp(self):
        import rtos_dashboard as dash
        self.dash = dash
        dash.events.clear()
        dash.obst.update(phase=None, detected=0, completed=0)

    def feed(self, *args, **kw):
        record = decode(self.TOPIC, obstacle_payload(*args, **kw))
        self.dash.ingest("obstacle", record)
        return record

    def test_phase_names_and_range_are_decoded(self):
        record = self.feed(2, servo=45, range_mm=183, detected=1)
        self.assertEqual(record["phase_name"], "AVOID_TURN_OUT")
        self.assertEqual((record["servo_deg"], record["range_mm"], record["detected"]), (45, 183, 1))

    def test_a_detour_makes_events_once(self):
        self.feed(0)                                   # following, nothing to report
        self.feed(1, detected=1)                       # obstacle seen
        self.feed(2, detected=1)                       # more of the same detour
        self.feed(0, detected=1, completed=1)          # back on the line
        self.feed(0, detected=1, completed=1)
        texts = [e["text"] for e in self.dash.events if e["system"] == "Obstacle"]
        self.assertEqual(len(texts), 2)
        self.assertIn("#1 detected", texts[0])
        self.assertIn("#1 complete", texts[1])

    def test_failure_is_a_fault_and_a_new_run_resets_counters(self):
        self.feed(1, detected=1)
        self.feed(26, detected=1)                      # AVOID_FAILED (25 is AVOID_RESTORE_FORWARD)
        self.assertEqual(self.dash.events[-1]["sev"], "fault")
        self.feed(0, active=False)                     # run ended
        self.feed(1, detected=1)                       # the next run's first obstacle reports again
        self.assertEqual(len([e for e in self.dash.events if "detected" in e["text"]]), 2)

    def test_malformed_phase_is_refused(self):
        with self.assertRaises(ValueError):
            decode(self.TOPIC, obstacle_payload(31))


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
