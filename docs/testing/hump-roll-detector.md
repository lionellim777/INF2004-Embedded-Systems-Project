# Roll-based hump detector and height estimate

The 10 mm hump (194 mm long, 94 mm up, 6.5 mm plateau, 94 mm down, 50 mm wide) sits under the
**right drive wheel only**. That tips the car sideways about 0.5 degrees per millimetre of lift
(roll about 4.6 degrees at 10 mm) but pitches it only about 3.5 degrees, so the existing pitch
detector (5 degree threshold) never sees it. `hump_roll.h` detects it from **roll** instead and
estimates the height. The pitch detector is unchanged and still runs.

## What it does

- Runs inside `terrain_poll()` on the same 20 ms samples and 8-sample filter as the pitch detector.
  It is informational: it changes no motion.
- Roll change = roll minus a baseline. The baseline starts from the level calibration (boot or GP21)
  and re-converges quickly during the first 2 seconds of every run (the 3 second countdown), then
  follows very slowly. This removes the 0.7 degree shifts seen between floor spots, which alone
  would cost about 1.4 mm of height.
- At the start of every run the baseline is **reset to the level calibration (0)**, not carried over
  from the previous run, and it only adapts while within 1.5 degrees of itself. A car left tilted
  (parked on a ramp, held in the hand) therefore cannot poison later runs. Press GP21 on the flat
  floor after moving or tilting the car, because a start tilted by more than 1.5 degrees will read
  as roll.
- **Launch hold-off:** `terrain_motion_go()` (called at `[MOTION GO]`) makes the detector ignore roll
  for 600 ms, because launch acceleration shows up as a roll swing of up to about 4.6 degrees.
  Any event in progress is dropped. Place the hump so its foot is at least about 100 mm from the
  start (the test layout of 80 mm still detects it in simulation, but 250 mm or more is safer).
- A hump starts when |roll change| stays above 2.5 degrees for 120 ms and ends when it stays below
  1.5 degrees for 200 ms. Events shorter than 350 ms are reported as `ROLL BUMP ignored`.
- Height comes from your measured paper-stack response (roll 4.62, 7.49, 10.16 degrees for 10, 15,
  20 mm), not from the ideal 110 mm geometry, which reads about 1 mm low at 10 mm.
- Right wheel raised gives negative roll (`side=RIGHT`), left wheel raised positive (`side=LEFT`).

## Log lines (USB serial and the dashboard's Raw log)

```
Terrain level roll=0.60 degrees (roll hump detector armed).
[HUMP] n=1 side=RIGHT height_mm=9.8 peak_roll=4.5 mean_roll=3.5 duration_ms=1100 baseline=0.55 highest_mm=9.8.
[HUMP IGNORED] reason=TOO_SHORT peak_roll=3.6 duration_ms=220 min_ms=350.
[HUMP IGNORED] reason=LEFT_SIDE peak_roll=4.3 duration_ms=507.
[HUMP IGNORED] reason=STILL_TILTED timeout_ms=10000.
```

Since 2026-11-17 these lines are tagged so the dashboard lists them in Events and the Hump board
reads them from the status line. The hump count, last height and highest height restart at zero
on every run (START press), so "highest" means the highest hump of the current run. Older logs
use the plain `ROLL HUMP n COMPLETE: ...` / `ROLL BUMP ignored` wording.

The end-of-run status line also carries `RollHumps=`, `HighestRollHumpMm=`, `LastRollHumpMm=` and
`RollChange=`.

## Verified so far (host only)

`python tools/test_hump_roll.py` ports the state machine to Python (constants are read from the
header) and drives it with a simulated hump using the measured sensor response, noise and a mis-set
baseline: 10 mm at 30 to 300 mm/s reads 9.0 to 9.9 mm, 15 mm reads 14.7, 20 mm reads 19.7, baseline
errors of plus or minus 0.7 degrees make no difference, level driving, a sustained 1.2 degree curve
and short bumps give no hump, two humps give two events, and a car stopped on the hump times out once.
The firmware builds with `-Wall -Wextra -Werror`. **Not yet run on the car**: motor vibration,
real approach speed and tyre behaviour on the real hump are unmeasured.

## Test on the car

1. Flash `build\motion-wireless\robot_firmware.uf2`. With the car flat, still and level
   on the floor, power it (it calibrates) or press GP21 while stopped.
2. **Measurement standard (user decision 2026-11-17): the hump is measured at 600 counts/s**, the
   line-following base cruise speed. Dashboard, Testing tab: pick **50 cm forward SLOW (hump at course
   speed)**, key `j`. Put the hump foot about 250 mm from the drive wheels, with the **right wheel on
   the hump's lane** and the caster off it. Press GP20, hold still for the 3 second countdown, let it
   drive (about 12 s). The **50 cm forward (hump test)** option (`k`) runs at about 2900 counts/s, where
   readings scatter from 6 to 16 mm: use it only to see the effect of speed, never to judge height.
3. Read the dashboard's Hump board (or the log). Expect one `[HUMP]` line, `side=RIGHT`, `peak_roll`
   about 4.7 to 5.1, `height_mm` about 9.3 to 9.8 for the 10 mm test hump.
4. Repeat 5 times, also with the car starting at different spots, and once with no hump (expect no
   roll events) and once with the hump under the left wheel (expect `side=LEFT`).

## Right side only, 350 ms minimum, height scale (2026-11-14)
- Flat-floor runs raised false LEFT events from the car's own sway (peaks 4-6 degrees). The hump
  is only under the right wheel, so only RIGHT events can complete; a long LEFT event is logged as
  `ROLL LEFT ignored`.
- Minimum duration is 350 ms (real humps ran 530-1030 ms; one real hump at 381 ms was rejected at 450 ms).
- Height is scaled by `HUMP_ROLL_HEIGHT_SCALE`. It was 0.80 for the fast `k` move (14 real runs averaged
  12.6 mm for a 10 mm hump, scatter about +/-2 mm). Since 2026-11-17 it is **0.91**, set from slow
  600 counts/s runs: 10 runs gave 9.2-10.2 mm, mean 9.7, sd 0.35 (last five: 9.3-9.8). Re-check after any change.
- Launch (2026-11-17): both wheels start at 0.30 PWM; the right-wheel feedforward is 1.0 on straight moves (0.95 on turns). This cut the left-wheel lead at launch from +43..61 to +14..24 counts.
