# Session handoff: roll-based hump detector (branch `ezra/ramp`, 2026-11-14)

Paste this into a new Claude Code session ("Read docs/testing/SESSION-HANDOFF-hump-detector.md and continue").
Nothing from this session is committed yet (`git status` shows many modified/untracked files).

## Project and workflow
- Pico W robocar firmware (INF2004). Wi-Fi dashboard for calibration/measurement; roll-based hump detector + height estimate in `firmware/app/robot_firmware/hump_roll.h`, wired into `terrain.c`. The detector is informational only (logs events; does not change motion).
- Workflow: user flashes `build/motion-wireless/robot_firmware.uf2`, does runs, saves logs under `C:\Users\etyte\School\Y2T1\Embedded Systems (INF2004)\Projects\Logs\{Flat-Floor,Hump}`. Claude reads the logs and tunes. The user says "hump" and "ramp" interchangeably.
- Test hump: 194 mm long (94 up / 6.5 plateau / 94 down), 10 mm high, 50 mm wide, under the RIGHT drive wheel only (left side is blocked by the barcode scanner). Foot 250 mm from the drive-wheel start. Use the dashboard option "50 cm forward (hump test)" (key `k`, cutoff 5964 counts, provisional). Left in log as "STRAIGHT_30CM".
- Per run: refresh dashboard (so each log holds one run), level calibration, "prepare run", run, save log. Power-cycle only after flashing.
- Build (no `cmake` on PATH): `PATH` with `C:\Users\etyte\.pico-sdk\ninja\v1.13.2`, then `C:\Users\etyte\.pico-sdk\cmake\v4.3.4\bin\cmake.exe --build build\motion-wireless`. Output `robot_firmware.uf2`. `robocar_motion_dashboard.uf2` in that folder is stale (19:26), safe to delete.
- Python: `C:\Users\etyte\AppData\Local\Programs\Python\Python313\python.exe -X utf8 tools\test_hump_roll.py` (passes). There is no host C compiler, so the C host tests (`firmware/tests/*`) have been edited but never run.
- Page regenerate: `python -X utf8 tools/embed_calibration_page.py` (without `-X utf8` it produces mojibake).

## What is implemented (all in the current build, 2026-11-14 23:14)
Detector (`hump_roll.h`): roll state machine WAITING/CANDIDATE/ACTIVE.
- ENTER 2.5 deg for 120 ms, EXIT 1.5 deg for 200 ms, MIN 350 ms, TIMEOUT 10 s, baseline reset at each run start and adapting only within EXIT, launch hold-off 600 ms after GO (`terrain_motion_go()`).
- Only RIGHT (negative roll) events complete; long LEFT events are `HUMP_ROLL_WRONG_SIDE` and logged `ROLL LEFT ignored`.
- Height = paper-stack table (roll 0/4.62/7.49/10.16 deg -> 0/10/15/20 mm) of the PEAK roll, times `HUMP_ROLL_HEIGHT_SCALE` 0.80. `mean_roll` is logged as a diagnostic only.
Motion (`motion_control.c/.h`, `course_control.c`, `encoders.c`):
- Straight-move launch ramp: floor 0.30 PWM (left 0.28), +0.9 PWM/s, straight only. It fixed the encoder-fault aborts caused by launch jerk.
- Encoder invalid-transition tolerance of 2 (`MOTION_ENCODER_INVALID_TOLERANCE`); ISR hardened (RAM function, highest IRQ priority).
- 50 cm option (`MOTION_STRAIGHT_50CM_STOP_COUNTS` 5964, provisional).

## Findings so far
- Flat floor: before the right-only filter, 5 of 9 runs raised false LEFT events (4-6 deg, 8-12 mm); not a floor feature (no repeat at the same distance, sign flips within a run). The car sways +/-2 to 4 deg while driving (accelerometer sees linear acceleration; the GY-511 has no gyro). After the filter: 0 false counted events in 5 flat runs.
- Hump runs: the first batch drifted upward (8 to 15 mm) because the hump was shifting; with the hump taped down the heights stayed steady. Over 14 earlier runs the old estimate averaged 12.6 mm (true 10), so scale 0.80 was added.
- Latest batch with scale 0.80 (new boot 77e7dabf): 11.6, 11.5, 7.9, 10.8, 11.3 mm (mean 10.6, spread about +/-1.5). Run 1 was not saved/registered (counter starts at 2).
- One real hump lasted 381 ms and was rejected at 450 ms; hence MIN 350 ms. Shortest accepted real hump 530 ms at about 190-200 mm/s. A faster car in the final course would shorten events and risk missing humps.
- Right drift at launch: the left wheel leads the right by about +25 to +50 counts at 500-700 counts of travel (was +52 before the left floor change). Samples are about 0.35 s apart, too coarse to tune.
- Stop jolt (descent plus braking in a 30/50 cm move) causes short "ROLL BUMP" events, correctly rejected.

## Open items (suggested order)
1. Read the course code and confirm it calls the run-start (`terrain_poll` gap > 1 s => `begin_run`) and `terrain_motion_go()` hooks for every segment, so hold-off and baseline reset happen in the real course.
2. Decide whether the course needs hump height for any decision; if so wire it in (the detector is informational only today).
3. Right drift: add a finer log of left-minus-right counts in the first second, then tune the left floor (0.28 is a guess).
4. Optional accuracy work: sway of +/-2-4 deg limits height to about +/-1.5-2 mm; more filtering would be needed to improve.
5. Housekeeping: delete stale `robocar_motion_dashboard.uf2`; run C host tests on a machine with a compiler (`firmware/tests`: motion_control_host_test, course_control_test, course_main_test, wireless_calibration_core_test); fix `build/measurement-diag-station` configure failure (never investigated); commit (nothing committed yet).

## Files changed this work (for the commit)
`firmware/app/robot_firmware/`: hump_roll.h (new), terrain.c/.h, motion_control.c/.h, course_control.c, encoders.c, robot_firmware.c, CMakeLists.txt, wireless/* (calibration core/html/page, `k` command), measurement.* and measurement_diag.c (new). `firmware/tests/`: motion_control_host_test.c, course_control_test.c, course_main_test.c, wireless_calibration_core_test.c. `tools/test_hump_roll.py` (new), `docs/testing/hump-roll-detector.md`.
