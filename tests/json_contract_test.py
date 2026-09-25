"""Parse the C serializer output with an independent JSON implementation."""

import json
import subprocess
import sys


result = subprocess.run(
    [sys.argv[1]], check=True, capture_output=True, text=True
)
lines = result.stdout.splitlines()
snapshot = json.loads(next(line[9:] for line in lines if line.startswith(
    "SNAPSHOT:")))
event = json.loads(next(line[6:] for line in lines if line.startswith(
    "EVENT:")))
full_snapshot = json.loads(next(line[14:] for line in lines if line.startswith(
    "FULL_SNAPSHOT:")))

assert snapshot["schema"] == 1
assert snapshot["mission"] is None
assert snapshot["obstacle"] is None
assert snapshot["motion"]["left_ticks"] == -(2**31)
assert snapshot["motion"]["right_ticks"] == (2**31 - 1)
assert snapshot["motion"]["distance_mm"] == (2**32 - 1)
assert event["kind"] == "scan_point"
assert event["angle_mdeg"] == 90000
assert full_snapshot["line"]["barcode"] == "?"
assert full_snapshot["line"]["ir"] == [65535, 65535, 65535]
assert full_snapshot["terrain"]["hump_mm"] == -(2**31)
assert full_snapshot["obstacle"]["right_clearance_mm"] == (2**32 - 1)
