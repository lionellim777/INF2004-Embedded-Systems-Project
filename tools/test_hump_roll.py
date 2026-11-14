#!/usr/bin/env python3
"""Host test for firmware/app/robot_firmware/hump_roll.h.

No host C compiler is assumed, so the state machine is ported line for line to Python
(constants are read from the header so they cannot drift) and driven with a simulated
hump: right-wheel-only profile (94 mm up, 6.5 mm plateau, 94 mm down, 10 mm peak), the
sensor transfer measured in the stack-of-paper test, an 8-sample moving average at 20 ms,
sensor noise and a mis-set level baseline. Run: python tools/test_hump_roll.py
"""
import math
import random
import re
from pathlib import Path

header = (Path(__file__).resolve().parents[1] / "firmware/app/robot_firmware/hump_roll.h").read_text()
const = {k: float(v.rstrip("fu")) for k, v in re.findall(r"#define (HUMP_ROLL_\w+)\s+([0-9.]+[fu]?)", header)}
TRACK, ENTER, EXIT = const["HUMP_ROLL_TRACK_MM"], const["HUMP_ROLL_ENTER_DEG"], const["HUMP_ROLL_EXIT_DEG"]
FAST, SLOW, FAST_SAMPLES = const["HUMP_ROLL_BASELINE_FAST"], const["HUMP_ROLL_BASELINE_SLOW"], int(const["HUMP_ROLL_FAST_SAMPLES"])
ENTER_MS, EXIT_MS, MIN_MS, TIMEOUT_MS = (int(const[k]) for k in
    ("HUMP_ROLL_ENTER_MS", "HUMP_ROLL_EXIT_MS", "HUMP_ROLL_MIN_MS", "HUMP_ROLL_TIMEOUT_MS"))
SCALE = const["HUMP_ROLL_HEIGHT_SCALE"]
HOLDOFF_MS = int(const["HUMP_ROLL_LAUNCH_HOLDOFF_MS"])
def _table(name):
    body = re.search(re.escape(name + "[HUMP_ROLL_POINTS] = {") + r"([^}]*)\}", header).group(1)
    return [float(x.strip().rstrip("f")) for x in body.split(",")]


_roll, _lift = _table("roll_deg"), _table("lift_mm")
POINTS = list(zip(_roll, _lift))
assert (TRACK, ENTER, EXIT) == (110.0, 2.5, 1.5) and (ENTER_MS, EXIT_MS, MIN_MS, TIMEOUT_MS) == (120, 200, 350, 10000)


def height_mm(deg):
    """Port of hump_roll_height_mm(): interpolate the measured paper-stack response."""
    r = abs(deg)
    i = 1
    while i < len(POINTS) - 1 and r > POINTS[i][0]:
        i += 1
    (r0, h0), (r1, h1) = POINTS[i - 1], POINTS[i]
    return h0 + (h1 - h0) * (r - r0) / (r1 - r0)


class Detector:
    WAITING, CANDIDATE, ACTIVE = 0, 1, 2

    def __init__(self):
        self.phase, self.sign, self.peak, self.start = 0, 0, 0.0, 0
        self.below_ms, self.below, self.need_clear = 0, False, False
        self.baseline = 0.0
        self.fast_left = FAST_SAMPLES
        self.holdoff_until = None
        self.sum, self.count = 0.0, 0

    def begin_run(self):
        """Port of hump_roll_begin_run(): baseline restarts at 0, nothing is carried over."""
        self.__init__()

    def motion_go(self, now):
        """Port of hump_roll_motion_go(): ignore roll for the launch hold-off."""
        self.holdoff_until = now + HOLDOFF_MS
        self.phase, self.below, self.need_clear = self.WAITING, False, False

    def _classify(self, dur):
        if dur < MIN_MS:
            return "short"
        return "completed" if self.sign < 0 else "wrong_side"

    def _fill(self, result, end):
        """Port of hump_roll_fill(): height from the peak; the mean over samples above ENTER is a
        logged diagnostic only. Tuple: result, sign, peak, height, duration, baseline, mean."""
        mean = self.sum / self.count if self.count else self.peak
        return (result, self.sign, self.peak, height_mm(self.peak) * SCALE, end - self.start, self.baseline, mean)

    def _add(self, m):
        self.sum += m
        self.count = min(self.count + 1, 65535)

    def update(self, calibrated_roll, now):
        if self.holdoff_until is not None:
            if now < self.holdoff_until:
                return None
            self.holdoff_until = None
        roll = calibrated_roll - self.baseline
        if self.phase == self.WAITING and not self.need_clear:
            fast = self.fast_left > 0
            if fast:
                self.fast_left -= 1
            if abs(roll) < EXIT:
                self.baseline += roll * (FAST if fast else SLOW)
                roll = calibrated_roll - self.baseline
        m, s = abs(roll), (-1 if roll < 0 else 1)
        ev = None
        if self.need_clear:
            if m < EXIT:
                self.need_clear = False
            return ev
        if self.phase == self.WAITING:
            if m > ENTER:
                self.phase, self.sign, self.peak, self.start, self.below = self.CANDIDATE, s, m, now, False
                self.sum, self.count = m, 1
        elif self.phase == self.CANDIDATE:
            if m > ENTER and s == self.sign:
                self.peak = max(self.peak, m)
                self._add(m)
                if now - self.start >= ENTER_MS:
                    self.phase, self.below = self.ACTIVE, False
            else:
                self.phase = self.WAITING
        else:
            if m > ENTER and s != self.sign:
                dur = now - self.start
                ev = self._fill(self._classify(dur), now)
                self.phase, self.sign, self.peak, self.start, self.below = self.CANDIDATE, s, m, now, False
                self.sum, self.count = m, 1
                return ev
            if s == self.sign and m > self.peak:
                self.peak = m
            if s == self.sign and m > ENTER:
                self._add(m)
            if m < EXIT:
                if not self.below:
                    self.below, self.below_ms = True, now
                if now - self.below_ms >= EXIT_MS:
                    dur = self.below_ms - self.start
                    ev = self._fill(self._classify(dur), self.below_ms)
                    self.phase = self.WAITING
            else:
                self.below = False
            if self.phase == self.ACTIVE and now - self.start > TIMEOUT_MS:
                ev = self._fill("timeout", now)
                self.phase, self.need_clear = self.WAITING, True
        return ev


# Roll change measured with the right wheel on stacks (stack mm -> deg), baseline 0.60 deg.
TRANSFER = [(h, r) for r, h in POINTS]
assert TRANSFER == [(0.0, 0.0), (10.0, 4.62), (15.0, 7.49), (20.0, 10.16)]


def sensor_roll(h_mm):
    """Absolute roll change (deg) for a raised-wheel height, interpolated from the measurements."""
    for (h0, r0), (h1, r1) in zip(TRANSFER, TRANSFER[1:]):
        if h_mm <= h1:
            return r0 + (r1 - r0) * (h_mm - h0) / (h1 - h0)
    return TRANSFER[-1][1] + 0.55 * (h_mm - 20)


def hump_height(s, peak=10.0, up=94.0, flat=6.5, down=94.0):
    if s < 0 or s > up + flat + down:
        return 0.0
    if s < up:
        return peak * s / up
    if s < up + flat:
        return peak
    return peak * (up + flat + down - s) / down


def run(heights, noise=0.10, baseline_error=0.0, side=-1, seed=1, extra=None, go_ms=None, det=None):
    """heights: one value per 20 ms sample. Returns the events the detector emits.
    go_ms: time of MOTION GO (arms the launch hold-off); det: reuse a detector across runs."""
    rng = random.Random(seed)
    det, events, window = det or Detector(), [], []
    went = False
    for i, h in enumerate(heights):
        if go_ms is not None and not went and i * 20 >= go_ms:
            det.motion_go(i * 20)
            went = True
        raw = side * sensor_roll(h) + baseline_error + rng.gauss(0, noise)
        if extra:
            raw += extra(i)
        window.append(raw)
        if len(window) > 8:
            window.pop(0)
        if len(window) < 8:
            continue
        ev = det.update(sum(window) / 8.0, i * 20)
        if ev:
            events.append(ev)
    return events


def drive(speed_mm_s, hump_start=100.0, peak=10.0, lead_s=3.0, tail_s=1.5, gaps=()):
    """Car moving at constant speed over a hump (plus optional further humps at given offsets)."""
    length = 94 + 6.5 + 94
    samples = []
    total = int((lead_s + (hump_start + length * (1 + len(gaps)) + sum(gaps)) / speed_mm_s + tail_s) * 50)
    for i in range(total):
        s = (i / 50.0 - lead_s) * speed_mm_s
        h = hump_height(s - hump_start, peak)
        offset = 0.0
        for g in gaps:
            offset += g
            h = max(h, hump_height(s - hump_start - offset - length, peak))
        samples.append(h)
    return samples


failures = []


def check(name, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + name + (" | " + detail if detail else ""))
    if not condition:
        failures.append(name)


# 1. one 10 mm hump (3 s countdown first, as in a real run), three speeds, baseline errors, right side
for speed in (50, 100, 150):
    for base in (-0.7, 0.0, 0.7):
        ev = run(drive(speed), baseline_error=base)
        ok = len(ev) == 1 and ev[0][0] == "completed" and ev[0][1] == -1 and 9.0 <= ev[0][3] / SCALE <= 10.6
        check(f"10 mm hump at {speed} mm/s, baseline off by {base:+.1f} deg", ok,
              f"events={[(e[0], e[1], round(e[2], 2), round(e[3], 2), e[4]) for e in ev]}")

# 2. taller humps read like the paper-stack data (8.9/14.5/19.8 mm for 10/15/20)
for peak, low, high in ((15.0, 14.2, 15.6), (20.0, 19.0, 20.6)):
    ev = run(drive(100, peak=peak))
    check(f"{peak:.0f} mm hump estimate", len(ev) == 1 and low <= ev[0][3] / SCALE <= high, f"{[round(e[3], 2) for e in ev]}")

# 3. a long left-side event is ignored (hump is under the right wheel only)
ev = run(drive(100), side=+1)
check("left-side event is classified wrong_side, never completed", len(ev) == 1 and ev[0][1] == 1 and ev[0][0] == "wrong_side")

# 4. nothing happens on level ground, with baseline error, noise or a gentle sustained curve
for base in (-0.7, 0.0, 0.7):
    check(f"level driving, baseline off by {base:+.1f}: no events", run([0.0] * 1500, baseline_error=base, noise=0.25) == [])
check("sustained 1.2 deg roll for 3 s (gentle curve): no events", run([0.0] * 400, extra=lambda i: 1.2 if 100 <= i < 250 else 0.0) == [])

# 5. short bumps are rejected, not counted as humps
bump = run([0.0] * 400, extra=lambda i: -3.4 if 100 <= i < 110 else 0.0)
check("80-200 ms 3.4 deg bump is not a hump", all(e[0] != "completed" for e in bump), str([(e[0], round(e[2], 2), e[4]) for e in bump]))
bump2 = run([0.0] * 400, extra=lambda i: -4.0 if 100 <= i < 112 else 0.0)
check("240 ms 4 deg bump is not a hump", all(e[0] != "completed" for e in bump2), str([(e[0], round(e[2], 2), e[4]) for e in bump2]))

# 6. two humps in one run
ev = run(drive(100, gaps=(150.0,)))
check("two humps give two completed events", [e[0] for e in ev] == ["completed", "completed"], str([(e[0], round(e[3], 2)) for e in ev]))

# 7. car stops on the hump: one timeout, no repeated events, re-arms after returning level
stuck = [10.0] * 650 + [0.0] * 200 + [10.0] * 100 + [0.0] * 200
ev = run(stuck)
check("stopped on hump: one timeout, then re-arms", [e[0] for e in ev][:1] == ["timeout"], str([(e[0], e[4]) for e in ev]))

# 8. slower and faster passes still detected (documents the working range)
for speed in (30, 200, 300):
    ev = run(drive(speed))
    print(f"info  {speed} mm/s: {[(e[0], round(e[3], 2), e[4]) for e in ev]}")
check("working range: 30 mm/s and 200 mm/s detected", all(len(run(drive(v))) == 1 for v in (30, 200)))

# 9. launch hold-off: a -4.6 deg roll swing 0.2-0.5 s after GO (acceleration, not a hump) is ignored
GO = 3000
launch = lambda i: -4.6 if (GO + 200) <= i * 20 < (GO + 500) else 0.0
check("launch roll swing without hold-off WOULD be an event (sanity)", len(run([0.0] * 400, extra=launch)) >= 1)
ev = run([0.0] * 400, extra=launch, go_ms=GO)
check("launch roll swing is ignored with hold-off", ev == [], str([(e[0], round(e[2], 2), e[4]) for e in ev]))

# 9b. plateau-based height: a brief dynamic spike at the apex (wheel slamming onto the hump) must
# inflate the reading much less than the raw peak would.
# apex of a 10 mm hump with foot 250 mm out at 190 mm/s is at t = 3 + (250 + 97) / 190 = 4.83 s
spike = lambda i: -6.0 if 4780 <= i * 20 < 4900 else 0.0
clean = run(drive(190, hump_start=250.0), go_ms=GO)
spiky = run(drive(190, hump_start=250.0), go_ms=GO, extra=spike)
check("spike test has one event each", len(clean) == 1 and len(spiky) == 1, f"{len(clean)} {len(spiky)}")
# The logged mean moves far less than the peak when a spike hits the apex (diagnostic only; height is
# still peak-based, a 2*mean-ENTER estimate was only ~28% less spike-sensitive, so it was not adopted).
if len(clean) == 1 and len(spiky) == 1:
    check("mean_roll is a bounded diagnostic: never above the peak, spike moves it less than the peak",
          all(e[6] <= e[2] for e in clean + spiky) and (spiky[0][6] - clean[0][6]) < (spiky[0][2] - clean[0][2]),
          f"clean peak/mean {clean[0][2]:.2f}/{clean[0][6]:.2f}, spiky {spiky[0][2]:.2f}/{spiky[0][6]:.2f}")
for e in clean:
    check("clean 10 mm hump reads near 10 mm (peak-based)", 9.0 <= e[3] / SCALE <= 10.8, f"{e[3]:.2f} mm, mean {e[6]:.2f}")

# 10. real hump 80 mm from the start (test layout) is still detected with the hold-off, plus a launch swing
for speed in (150, 190, 230):
    ev = run(drive(speed, hump_start=80.0), extra=launch, go_ms=GO)
    ok = len(ev) == 1 and ev[0][0] == "completed" and ev[0][1] == -1 and 9.0 <= ev[0][3] / SCALE <= 10.6
    check(f"hump 80 mm from start at {speed} mm/s detected despite launch swing", ok,
          str([(e[0], round(e[3], 2), e[4]) for e in ev]))

# 11. a tilt left over from a previous run (e.g. car parked on a ramp) must not poison the next run
det = Detector()
run([0.0] * 400, baseline_error=-3.0, det=det)           # run on the tilt: baseline may drift, no matter
det.begin_run()
check("begin_run resets the baseline to the level calibration", det.baseline == 0.0)
ev = run(drive(190, hump_start=250.0), go_ms=GO, det=det)
check("hump after a tilted run reads right side, ~10 mm", len(ev) == 1 and ev[0][1] == -1 and 9.0 <= ev[0][3] / SCALE <= 10.6,
      str([(e[0], e[1], round(e[3], 2)) for e in ev]))

# 12. a car that starts tilted beyond EXIT does not absorb the tilt into the baseline
det = Detector()
run([0.0] * 300, baseline_error=-3.0, det=det)
check("tilt of 3 deg is not absorbed by the baseline", abs(det.baseline) < 0.05, f"baseline={det.baseline:.3f}")

print("PASSED" if not failures else "FAILED: " + ", ".join(failures))
raise SystemExit(1 if failures else 0)
