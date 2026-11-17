#!/usr/bin/env python3
"""Host test for firmware/app/robot_firmware/tilt_alert.h.

No host C compiler is assumed, so the state machine is ported line for line to Python (constants
are read from the header so they cannot drift). Run: python tools/test_tilt_alert.py

The last test replays the roll traces saved by the car ([ROLL TRACE] lines in the hump and
flat-floor logs) when they are present: none of them may raise a tilt alert.
"""
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "firmware/app/robot_firmware/tilt_alert.h").read_text()
const = {k: float(v.rstrip("fu")) for k, v in re.findall(r"#define (TILT_ALERT_\w+)\s+([0-9.]+[fu]?)", header)}
ENTER, EXIT = const["TILT_ALERT_ENTER_DEG"], const["TILT_ALERT_EXIT_DEG"]
ENTER_MS, EXIT_MS = int(const["TILT_ALERT_ENTER_MS"]), int(const["TILT_ALERT_EXIT_MS"])
assert (ENTER, EXIT, ENTER_MS, EXIT_MS) == (15.0, 10.0, 400, 300)


class Tilt:
    def __init__(self):
        self.active = self.above = self.below = False
        self.since = self.below_ms = 0
        self.peak = self.peak_pitch = self.peak_roll = 0.0

    def update(self, pitch, roll, now):
        """Returns None, ("raised", tilt, pitch, roll) or ("cleared", peak, pitch, roll, duration)."""
        tilt = math.hypot(pitch, roll)
        if not self.active:
            if tilt <= ENTER:
                self.above = False
                return None
            if not self.above:
                self.above, self.since, self.peak = True, now, 0.0
            if tilt > self.peak:
                self.peak, self.peak_pitch, self.peak_roll = tilt, pitch, roll
            if now - self.since >= ENTER_MS:
                self.active, self.above, self.below = True, False, False
                return ("raised", tilt, pitch, roll)
            return None
        if tilt > self.peak:
            self.peak, self.peak_pitch, self.peak_roll = tilt, pitch, roll
        if tilt < EXIT:
            if not self.below:
                self.below, self.below_ms = True, now
            if now - self.below_ms >= EXIT_MS:
                ev = ("cleared", self.peak, self.peak_pitch, self.peak_roll, self.below_ms - self.since)
                self.active = self.below = self.above = False
                return ev
        else:
            self.below = False
        return None


failures = []


def check(name, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + name + (" | " + detail if detail else ""))
    if not condition:
        failures.append(name)


def drive(profile, step_ms=21, gap_every=5, gap_ms=60, end_ms=None):
    """profile(t_ms) -> (pitch, roll). Samples every 21 ms with a 60 ms gap every fifth sample, as
    the car's sonar read does. Returns the list of events with their times."""
    t, n, tilt, events = 0, 0, Tilt(), []
    end_ms = end_ms or 6000
    while t < end_ms:
        ev = tilt.update(*profile(t), t)
        if ev:
            events.append((t, ev))
        n += 1
        t += gap_ms if n % gap_every == 0 else step_ms
    return events, tilt


# A hump at course speed: about -5.5 deg roll for 1.5 s, under 4 deg of pitch.
events, _ = drive(lambda t: (3.5, -5.5) if 1000 < t < 2500 else (0.0, 0.0))
check("10 mm hump at course speed raises no alert", not events, str(events))

# A 20 mm hump (top of the measured range): about -10.2 deg roll and 4 deg pitch, 11 deg combined.
events, _ = drive(lambda t: (4.0, -10.2) if 1000 < t < 2500 else (0.0, 0.0))
check("20 mm hump (about 11 deg combined) raises no alert", not events, str(events))

# The fast 'k' hump peaked at about -10 deg roll with about 6 deg of pitch (11.7 deg combined).
events, _ = drive(lambda t: (6.0, -10.0) if 1000 < t < 2500 else (0.0, 0.0))
check("fast hump (11.7 deg combined) raises no alert", not events, str(events))

# Tip: 18 deg held. The alert rises 400 ms after the tilt first exceeds ENTER, then clears 300 ms
# after it drops under EXIT, reporting the peak and the duration.
events, _ = drive(lambda t: (18.0, 0.0) if 1000 <= t < 3000 else (0.0, 0.0))
kinds = [e[1][0] for e in events]
check("sustained 18 deg raises one alert and clears it once", kinds == ["raised", "cleared"], str(events))
if len(events) == 2:
    check("alert rises about 400 ms after the tilt starts", 1400 <= events[0][0] <= 1500, str(events[0][0]))
    check("clear reports the 18 deg peak", abs(events[1][1][1] - 18.0) < 0.01, str(events[1]))
    check("clear comes about 300 ms after the tilt ends", 3300 <= events[1][0] <= 3400, str(events[1][0]))

# A 200 ms spike is a knock, not a tilt.
events, _ = drive(lambda t: (20.0, 0.0) if 1000 <= t < 1200 else (0.0, 0.0))
check("200 ms spike to 20 deg raises no alert", not events, str(events))

# Pitch and roll combine: 11 + 11 is 15.6 deg.
events, _ = drive(lambda t: (11.0, 11.0) if 1000 <= t < 2000 else (0.0, 0.0))
check("combined pitch and roll above ENTER raises an alert", [e[1][0] for e in events][:1] == ["raised"], str(events))
events, _ = drive(lambda t: (7.0, 7.0) if 1000 <= t < 2000 else (0.0, 0.0))
check("combined 9.9 deg is below ENTER", not events, str(events))

# Hysteresis: between EXIT (10) and ENTER (15) the alert stays up; a short dip below EXIT does not clear it.
events, tilt = drive(lambda t: (12.0, 0.0) if 3000 <= t < 4000 else ((18.0, 0.0) if 1000 <= t < 3000 else (0.0, 0.0)),
                     end_ms=3900)
check("alert stays raised at 12 deg (between EXIT and ENTER)", tilt.active and [e[1][0] for e in events] == ["raised"], str(events))
events, tilt = drive(lambda t: (0.0, 0.0) if 2000 <= t < 2200 else (18.0, 0.0), end_ms=4000)
check("a 200 ms dip under EXIT does not clear the alert", tilt.active and [e[1][0] for e in events] == ["raised"], str(events))

# A second tilt after clearing raises a second alert.
events, _ = drive(lambda t: (18.0, 0.0) if 1000 <= t < 2000 or 4000 <= t < 5000 else (0.0, 0.0), end_ms=7000)
check("a second tilt raises a second alert", [e[1][0] for e in events] == ["raised", "cleared", "raised", "cleared"], str(events))

# Replay the roll traces saved by the car: hump and flat-floor runs must never alert.
logs = ROOT.parent.parent.parent / "Logs"
count = 0
alerts = []
for path in sorted(logs.glob("*/*.txt")) if logs.exists() else []:
    text = path.read_text(encoding="utf-8", errors="replace")
    heads = list(re.finditer(r"\[ROLL TRACE\] samples=(\d+).*?baseline=([-\d.]+)", text))
    if not heads:
        continue
    head = heads[-1]
    n, base = int(head.group(1)), float(head.group(2))
    pts = []
    for m in re.finditer(r"\[ROLL TRACE (\d+)\]((?: \d+:-?\d+/-?\d+)+)", text[head.end():]):
        pts += [(int(t), int(a) / 10 - base) for t, a, _ in re.findall(r"(\d+):(-?\d+)/(-?\d+)", m.group(2))]
    tilt = Tilt()
    count += 1
    for t, roll in pts[:n]:
        if tilt.update(0.0, roll, t):
            alerts.append(path.name)
check(f"{count} saved roll traces raise no alert", count == 0 or not alerts,
      "no logs found" if count == 0 else str(alerts))

print("PASSED" if not failures else "FAILED: " + ", ".join(failures))
raise SystemExit(1 if failures else 0)
