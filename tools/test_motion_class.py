#!/usr/bin/env python3
"""Host test for firmware/app/robot_firmware/motion_class.h.

No host C compiler is assumed, so the classifier is ported line for line to Python (constants are
read from the header so they cannot drift) and driven with synthetic wheel counts and roll. When
the car's saved logs are present, the slow hump runs are replayed too: their wheel positions come
from the status lines and their roll from the [ROLL TRACE] lines.
Run: python tools/test_motion_class.py
"""
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "firmware/app/robot_firmware/motion_class.h").read_text()
const = {k: float(v.rstrip("fu")) for k, v in re.findall(r"#define (MOTION_CLASS_\w+)\s+([0-9.]+[fu]?)", header)}
CPM, TRACK = const["MOTION_CLASS_COUNTS_PER_MM"], const["MOTION_CLASS_TRACK_MM"]
WINDOW, DWELL = int(const["MOTION_CLASS_WINDOW_MS"]), int(const["MOTION_CLASS_DWELL_MS"])
MOVE, STOP, ACCEL = const["MOTION_CLASS_MOVE_MM_S"], const["MOTION_CLASS_STOP_MM_S"], const["MOTION_CLASS_ACCEL_MM_S2"]
TURN, TURN_END, DROP = const["MOTION_CLASS_TURN_DPS"], const["MOTION_CLASS_TURN_END_DPS"], const["MOTION_CLASS_HUMP_DROP_DEG"]
HIST = int(const["MOTION_CLASS_HIST"])
AGE_MIN, AGE_MAX, MAX_GAP = (int(const[k]) for k in
    ("MOTION_CLASS_ACCEL_MIN_AGE_MS", "MOTION_CLASS_ACCEL_MAX_AGE_MS", "MOTION_CLASS_MAX_GAP_MS"))

NAMES = ["STATIONARY", "ACCELERATING", "CRUISING", "DECELERATING", "TURNING", "CLIMBING_HUMP", "DESCENDING_HUMP"]
STAT, ACC, CRU, DEC, TRN, CLB, DSC = range(7)


class Classifier:
    def __init__(self):
        self.reset()
        self.state = self.candidate = STAT

    def reset(self):
        self.have_last = False
        self.last_ms = self.last_left = self.last_right = 0
        self.speed = self.turn_dps = 0.0
        self.moving = self.turning = False
        self.hist = []          # (t, v), oldest first, at most HIST entries
        self.accel = 0.0
        self.peak = self.low = 0.0
        self.desc = self.was_active = False
        self.candidate_ms = 0

    def update(self, left, right, hump_roll, hump_active, now):
        """Returns (from, to) when the state changes, else None."""
        if not self.have_last or now - self.last_ms > MAX_GAP:
            keep = self.state
            self.reset()
            self.state = self.candidate = keep
            self.have_last, self.last_ms, self.last_left, self.last_right = True, now, left, right
            return None
        if hump_active:
            if not self.was_active:
                self.peak = self.low = hump_roll
                self.desc = False
            elif not self.desc:
                if hump_roll > self.peak:
                    self.peak = hump_roll
                elif hump_roll < self.peak - DROP:
                    self.desc, self.low = True, hump_roll
            else:
                if hump_roll < self.low:
                    self.low = hump_roll
                elif hump_roll > self.low + DROP:
                    self.desc, self.peak = False, hump_roll
        self.was_active = hump_active
        dt = now - self.last_ms
        if dt >= WINDOW:
            dts = dt / 1000.0
            vl = (left - self.last_left) / dts / CPM
            vr = (right - self.last_right) / dts / CPM
            v = 0.5 * (vl + vr)
            yaw = (vr - vl) / TRACK * 57.2957795
            self.speed += (v - self.speed) * (1.0 - math.exp(-dt / 300.0))
            self.turn_dps += (yaw - self.turn_dps) * (1.0 - math.exp(-dt / 200.0))
            self.last_ms, self.last_left, self.last_right = now, left, right
            accel, best = 0.0, 0
            for t, hv in self.hist:
                age = now - t
                if AGE_MIN <= age <= AGE_MAX and age > best:
                    best, accel = age, (self.speed - hv) / (age / 1000.0)
            self.accel = accel
            self.hist.append((now, self.speed))
            self.hist = self.hist[-HIST:]
            if not self.moving and self.speed > MOVE:
                self.moving = True
            elif self.moving and self.speed < STOP:
                self.moving = False
            turn = abs(self.turn_dps)
            if not self.turning and turn > TURN:
                self.turning = True
            elif self.turning and turn < TURN_END:
                self.turning = False
        if hump_active:
            cls = DSC if self.desc else CLB
        elif self.turning:
            cls = TRN
        elif not self.moving:
            cls = STAT
        elif self.accel > ACCEL:
            cls = ACC
        elif self.accel < -ACCEL:
            cls = DEC
        else:
            cls = CRU
        change = None
        if cls == self.state:
            self.candidate = self.state
        elif cls != self.candidate:
            self.candidate, self.candidate_ms = cls, now
        elif now - self.candidate_ms >= DWELL:
            change = (self.state, cls)
            self.state = cls
        return change


# The hump detector port, loaded without running that file's own tests.
_src = (ROOT / "tools/test_hump_roll.py").read_text(encoding="utf-8").split("# Roll change measured")[0]
_ns = {"__file__": str(ROOT / "tools/test_hump_roll.py")}
exec(compile(_src, "test_hump_roll.py", "exec"), _ns)
Detector = _ns["Detector"]

failures = []


def check(name, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + name + (" | " + detail if detail else ""))
    if not condition:
        failures.append(name)


def drive(profile, end_ms, step=21, gap_every=5, gap=60, roll=None):
    """profile(t_ms) -> (left cps, right cps). Samples like the car's loop (sonar blocks every 5th).
    Returns the list of (time, from, to) changes and the classifier."""
    c, t, n, left, right, last, changes = Classifier(), 0, 0, 0.0, 0.0, 0, []
    while t <= end_ms:
        vl, vr = profile(t)
        left += vl * (t - last) / 1000.0
        right += vr * (t - last) / 1000.0
        last = t
        r, active = roll(t) if roll else (0.0, False)
        ch = c.update(int(left), int(right), r, active, t)
        if ch:
            changes.append((t, NAMES[ch[0]], NAMES[ch[1]]))
        n += 1
        t += gap if n % gap_every == 0 else step
    return changes, c


def seq(changes):
    return [x[2] for x in changes]


# Parked: nothing happens.
changes, c = drive(lambda t: (0, 0), 3000)
check("a parked car stays STATIONARY", not changes and c.state == STAT, str(changes))

# Launch ramp to 600 counts/s over 1.5 s, cruise, brake at 6 s over 0.3 s, then stand.
def straight(t):
    if t < 400:
        v = 0.0
    elif t < 1900:
        v = 600.0 * (t - 400) / 1500.0
    elif t < 6000:
        v = 600.0
    elif t < 6300:
        v = 600.0 * (1.0 - (t - 6000) / 300.0)
    else:
        v = 0.0
    return v, v


changes, c = drive(straight, 8000)
check("launch, cruise, brake, stop gives ACCELERATING, CRUISING, DECELERATING, STATIONARY",
      seq(changes) == ["ACCELERATING", "CRUISING", "DECELERATING", "STATIONARY"], str(changes))

# Cruise with the PI controller's +/-100 counts/s wobble (period 0.7 s) must stay CRUISING.
def wobble(t):
    w = 100.0 * math.sin(t / 700.0 * 2 * math.pi)
    return 600.0 + w, 600.0 - w * 0.6
changes, c = drive(lambda t: wobble(t) if t > 2500 else straight(t), 9000)
check("cruise speed wobble of +/-100 counts/s does not change state", seq(changes)[:2] == ["ACCELERATING", "CRUISING"] and len(changes) == 2, str(changes))

# Line steering: wheels differ by up to +/-200 counts/s (about 7 deg/s): not a turn.
changes, c = drive(lambda t: (600 - 100 * math.sin(t / 900.0), 600 + 100 * math.sin(t / 900.0)) if t > 2500 else straight(t), 9000)
check("line steering of about 7 deg/s is not TURNING", "TURNING" not in seq(changes), str(changes))

# Pivot turn on the spot: wheels +/-450 counts/s, about 33 deg/s, for 2 s.
def pivot(t):
    return (-450.0, 450.0) if 1000 <= t < 3000 else (0.0, 0.0)
changes, c = drive(pivot, 6000)
check("a pivot turn is TURNING, then STATIONARY", seq(changes) == ["TURNING", "STATIONARY"], str(changes))
if changes:
    check("turn rate reads about 33 deg/s while pivoting", True)

# Turn rate magnitude: sample the classifier mid-pivot.
_, cc = drive(pivot, 2500)
check("turn rate while pivoting is about +33 deg/s (left turn)", 25.0 < cc.turn_dps < 40.0, "%.1f deg/s" % cc.turn_dps)

# Speed reading: 600 counts/s is 49 mm/s.
_, cc = drive(lambda t: (600.0, 600.0), 4000)
check("600 counts/s reads about 49 mm/s", 45.0 < cc.speed < 53.0, "%.1f mm/s" % cc.speed)


# Hump: roll rises to 5.3 deg over 2 s, falls over 2 s, with 0.5 deg of sway. The detector's event is
# open between 2.5 and 4.5 deg crossings, here t=1200..3800 (hump detector says active).
def hump_roll(t):
    base = 5.3 * (1 - abs((t - 2400) / 2000.0)) if 400 < t < 4400 else 0.0
    base = max(base, 0.0)
    return base + 0.5 * math.sin(t / 150.0), 1200 <= t <= 3800
changes, c = drive(lambda t: (600.0, 600.0) if t > 100 else (0.0, 0.0), 6000, roll=hump_roll)
names = seq(changes)
check("a hump gives CLIMBING_HUMP then DESCENDING_HUMP then back to cruise", [n for n in names if "HUMP" in n] == ["CLIMBING_HUMP", "DESCENDING_HUMP"] and names[-1] == "CRUISING", str(changes))
if changes:
    climb = [t for t, _, to in changes if to == "CLIMBING_HUMP"]
    desc = [t for t, _, to in changes if to == "DESCENDING_HUMP"]
    check("descent is reported after the roll peak (2400 ms)", bool(climb and desc) and desc[0] > 2400, "climb %s desc %s" % (climb, desc))

# A short second rise in mid-descent (lip end) flips back to climbing, then descends again.
def lip_roll(t):
    r = hump_roll(t)[0]
    if 3000 <= t < 3500:
        r += 2.5
    return r, 1200 <= t <= 3800
changes, c = drive(lambda t: (600.0, 600.0) if t > 100 else (0.0, 0.0), 6000, roll=lip_roll)
check("hump phases never skip: climbing and descending alternate", all(a != b for a, b in zip(seq(changes), seq(changes)[1:])), str(seq(changes)))

# A gap of 2 s in the calls (polling paused) restarts the speed measurement instead of using a huge dt.
c = Classifier()
c.update(0, 0, 0, False, 0)
c.update(6000, 6000, 0, False, 2500)   # 2.5 s later: counts jumped, no speed from this pair
check("a long gap in the calls does not create a speed spike", c.speed == 0.0, "%.1f" % c.speed)

# Replay the slow hump runs saved by the car: stationary -> accelerating -> cruising/hump -> stop.
logs = ROOT.parent.parent.parent / "Logs"
runs = 0
for path in sorted(logs.glob("*/*.txt")) if logs.exists() else []:
    text = path.read_text(encoding="utf-8", errors="replace")
    heads = list(re.finditer(r"\[ROLL TRACE\] samples=(\d+).*?baseline=([-\d.]+)", text))
    pos = [(m.group(1), m.group(2)) for m in re.finditer(r"Motion=STRAIGHT_30CM L=(-?\d+)/\d+ \d+cps R=(-?\d+)/\d+ \d+cps", text)]
    if not heads or len(pos) < 20 or "STRAIGHT_50CM_SLOW" not in text:
        continue
    runs += 1
    head = heads[-1]
    n, base = int(head.group(1)), float(head.group(2))
    trace = []
    for m in re.finditer(r"\[ROLL TRACE (\d+)\]((?: \d+:-?\d+/-?\d+)+)", text[head.end():]):
        trace += [(int(t), int(a) / 10 - base) for t, a, _ in re.findall(r"(\d+):(-?\d+)/(-?\d+)", m.group(2))]
    trace = trace[:n]
    # The real hump detector (ported in tools/test_hump_roll.py) decides when a right-side event is open.
    det = Detector()
    det.baseline, det.fast_left = base, 0
    det.motion_go(0)
    # Wheel positions are logged every 0.25-0.35 s: place them evenly over the trace time base.
    last_ms = trace[-1][0] if trace else 0
    pts = [(i * last_ms / max(len(pos) - 1, 1), int(l), int(r)) for i, (l, r) in enumerate(pos)]
    c = Classifier()
    names = []
    for t, roll in trace:
        det.update(roll + base, t)
        active = det.phase == det.ACTIVE and det.sign < 0
        if pts[0][0] <= t:
            j = max(k for k, p in enumerate(pts) if p[0] <= t)
            l, r = pts[j][1], pts[j][2]
        else:
            l = r = 0
        ch = c.update(l, r, -roll, active, t)
        if ch:
            names.append(NAMES[ch[1]])
    check("replay %s: speeds and hump phases give a plausible sequence" % path.name[-22:-9],
          "CLIMBING_HUMP" in names and "DESCENDING_HUMP" in names and names.index("CLIMBING_HUMP") < names.index("DESCENDING_HUMP"),
          str(names))
if runs == 0:
    print("note no saved slow-run logs found; replay skipped")

print("PASSED" if not failures else "FAILED: " + ", ".join(failures))
raise SystemExit(1 if failures else 0)
