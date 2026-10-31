# Motion integration — 21 October 2026

Wireless continuation: **v5.2 wireless**, with a provisional R90-only +58-count trim,
combined sensor summaries and an independent motor service guard. Other tuning stays unchanged.
See [wireless calibration instructions](wireless-calibration.md) for the new UF2,
car Wi-Fi network and browser logs. See [completed turn evidence and next combined check](testing/motion-wireless-v5_2-combined-check.md).

USB-only baseline build: **v4**. v3 travelled a reported 30 cm with minimal error but had
a slight end-left pivot. v4 retains the 3537-count endpoint and adds bounded
straight-wheel position balancing plus cutoff timestamps. Turn tuning stays
unchanged. See [v3 evidence and v4 change](testing/motion-integration-stop-yaw-v4.md).
The changes below describe the original v1 integration baseline except where
the version note overrides them. v4 physical handling is still pending.

This build continues from the completed component tests. It does not restart
encoder, motor, servo, sonar or IR calibration.

## Changes

- Ported Batch 5 into `motion_control.c`. It computes PWM requests without GPIO
  writes, waiting, printing or counter resets. Main retains motor arbitration.
- Preserved CPR 2543, wheel 60/track 115 mm, KP .000035/KI .000015, integral
  clamp ±2500, 100 ms control interval, speed ramp 1800/1200/600 counts/s,
  feedforward/right trim, PWM bounds 0.25..0.65, sync gain 1.5/limit 120,
  turn correction 290, left 90 +65 and right 180 −60 counts.
- Added an atomic encoder pair snapshot and invalid-transition counts. Raw ISR
  counts wrap unsigned; manoeuvres use relative positions without resetting them.
- USB selects a diagnostic motion while stopped. Only GP20 starts it, after a
  three-second countdown. GP20 aborts. Every motion stops after completion and
  reports its settled counts after 500 ms. No next motion starts automatically.
- Existing IR/sonar/terrain code remains. Barcode A/B/D now requests a countdown,
  encoder turn, then STOP. GP20 resumes line following after a barcode turn.
- New fail stops: new invalid transitions, >64 counts wrong direction, one second
  without encoder progress, >250 ms control-update delay, and 12 s motion timeout.
- Terrain I2C transactions now time out after 2 ms rather than waiting indefinitely;
  USB output timeout is 1 ms to reduce serial-induced stalls.
- Main uses the packed barcode reader (132 ->88 bytes per instance). Motion
  working values remain native width. Full 1,352-byte implementation is pending.

## First on-car check

1. With motor power off, flash
   `build/motion-integration/robocar_motion_integration_v4.uf2`. Use the existing
   VS Code serial extension at 115200. No separate monitor is launched.
2. Confirm the `AAI10 MOTION INTEGRATION v4` banner and stopped state. Startup
   still includes stationary/level terrain zeroing as before. `?` repeats help.
3. Place the car on a clear floor and enable motor power. Send `f` while stopped.
   Expect `[SELECT] STRAIGHT_30CM. No movement until GP20.`
4. Press GP20. Watch the countdown, `[MOTION GO]`, positions, `[MOTION STOP]`
   and `[MOTION SETTLED]`. GP20 aborts during countdown or movement.
5. Save the settled line and record whether it travelled straight and stopped
   near the previous 30 cm result. This checks integration against existing
   calibration; it is not another hand-rotation calibration.

Then select `l`/`r`/`u` for the previously calibrated turns, one at a time.
Send `n` to select the original line-follow mode. Selection never moves the car.
`x` stops/disarms when the main loop processes it; GP20 is required to restart.
GP21 retains stopped-only level calibration.

During selected encoder motions IR remains reported but does not steer/cancel
them. Sonar stop remains active; no echo is unknown, not proof of clearance.
There is no obstacle bypass, rear sensor or Wi-Fi/MQTT in this build.

## Verification and limits

- Host ASan/UBSan checks passed: preserved target pairs 4047/4047, −994/994,
  929/−929 and 2087/−2087; independently calculated first PI outputs; four
  simulated manoeuvres; count wrap; independent wheel completion; abort/no
  restart; request validation; and all new controller fault paths.
- Barcode regression passed: old pipeline, 156 valid letter/direction/scale
  cases, 5,000 reference comparisons, storage boundaries and neighbouring fields.
- Full Pico W build passed with Pico SDK 2.1.1 and Arm GNU 14.2.Rel1, using
  `-mcpu=cortex-m0plus -mthumb -Wall -Wextra -Werror`.
- Subsequent first physical straight run: completed with settled 4272/4144 and
  invalid 0/0. Operator-estimated 35 cm was not ruler-measured. See
  [run evidence](testing/motion-integration-2026-10-21.md). Measured distance,
  stopping latency, repeatability and full-course acceptance remain open.

The controller is nonblocking. The overall polling prototype still uses bounded
blocking sonar reads and startup terrain zeroing. It is not the six-task RTOS
application; a 50 ms stop guarantee has not been established. The new fault
thresholds still need physical validation for false stops. Native motion working
fields remain until compact precision/range/session policies are implemented.
The 1,352-byte target is not this image's total RAM consumption.
