# v5.3 dedicated barcode and bounded two-sensor line check

## Evidence and physical roles

User's 26 October ordered placement and explicit identification establish:

| Physical role (viewed from behind, facing forward) | Existing log label | GPIO |
|---|---|---|
| Offset left barcode sensor | IR_L / first IR value | GP26 |
| Left sensor of central line pair | IR_C / second value | GP28 |
| Right sensor of central line pair | IR_R / third value | GP27 |

Raw log: `serial-output/20261026-v5_2-ir-role-confirmation.txt`.
SHA-256: `95ab4ccd6e2e05e84256f40d98c3a483e8ff38f6a27c92a905c85ba74593cf40`.
It includes the previous unordered sweep, then the latest placements: first channel
high around lines 430–458, second around 460–485, third from 486. Transitions include
mixed readings; the physical-role assignment relies on the user's explicit labels
as well as channel response, not guessing from signal amplitude.

## Firmware changes

`line_check.h` maps the first channel only to barcode input, and second/third to
line-left/line-right. Existing calibrated IR reference values and detection threshold
remain. Barcode capture now consumes only GP26 instead of a two-of-three vote.
Barcode results are logged without triggering motion in this diagnostic build.
Sonar polling remains enabled during barcode capture; blocking sonar can affect
barcode edge sampling, so successful full-speed decoding is not claimed.

The new browser `t` selection (USB `t` or legacy `n`) requires GP20 and a three-second
countdown. The two-second check drives forward with these diagnostic PWM settings:

| Line-left / line-right black | Left / right PWM | Interpretation |
|---|---|---|
| No / no | 0.40 / 0.40 | Line between sensors OR line absent; ambiguous |
| Yes / no | 0.25 / 0.55 | Correct left |
| No / yes | 0.55 / 0.25 | Correct right |
| Yes / yes | 0 / 0 | Stop for unresolved broad black/junction |

Stops at the first observed two-second limit, 3537 forward counts on either wheel,
both central sensors black, any invalid encoder transition, or reverse motion beyond
64 counts. GP20/web/USB abort, sonar stop and the independent motor-service guard
remain active. The time/count limit is checked in the foreground and is subject to
loop delay; it is not an exact ground-distance promise. There is no full line-loss
or junction-recovery claim. No unattended repeat or automatic barcode turn.

The two-white ambiguity is inherent to this geometry. A finite straight diagnostic
may continue through it; final navigation must resolve gaps/line-loss using additional
state/context and tested limits. This mode replaces the previous unbounded USB `n`
prototype, not a completed mission controller.

All four calibrated movement targets and PI/straight-balance constants remain:
straight3537/3537, left90-994/994, right90 987/-987, right1802087/-2087.
Every run retains motion, IR, sonar, pitch and timing summaries; labels preserve the
old channel order. New live `Roles=Barcode/LineLeft/LineRight` booleans clarify use.

## Verification and next physical check

Host tests cover role separation, barcode independence of steering, steering direction,
two-white ambiguity, both-black stop, time/count boundaries and encoder fault stops.
Existing motion and barcode pipeline tests, wireless protocol tests, mock browser
selection/download/reconnect tests, wireless and USB-only Pico W builds, and UF2
structure/family/address/banner checks are used. Hardware validation is pending.

Flash `build/motion-wireless/robocar_motion_integration_v5_3_wireless.uf2`, keep the
power bank secured, and refresh the same AP page to v5.3. On a straight line with at
least 60 cm of clear track ahead, place the line between the central pair with the
offset sensor over plain floor. Select Line check, press GP20, observe corrections,
and download after CHECK/TIMING. Stop if it turns away or leaves the line. A time or
travel-limit result ends this diagnostic normally; it does not mean course completion.
No need to repeat the motor calibration or stationary IR sweeps.
