> **v5.47 current:** Conditional 600–800 cps stable straight cruise and faster clear MOVE_OUT/MOVE_ALONG legs. Slow bend/rejoin/turn behaviour retained. v5.46 physically completed one bend bypass and followed to the tape end; full straight-strip return and new speed profile await user tests. Software checks/build passed.

> **v5.46 current:** Perpendicular rejoin turns right first. Early black readings outside the 60–115 degree alignment window are rejected; strong tape must persist for 80 ms, then survive stopped confirmation. A vanished candidate resumes the remaining sweep rather than immediately failing. Opposite search remains a bounded fallback. Software regressions/build passed; physical acceptance pending. Earlier entries below are historical.

> **v5.45 current:** Both v5.44 runs stalled before reaching the alignment turn. Probe and axle advance now use adaptive speed control with up to 32% effort and bounded breakaway assistance; one-wheel lag holds the leading wheel. Stall stops remain. The 20 cm inspection spacing and v5.44 perpendicular alignment sequence remain. Software load/geometry regressions passed; physical run pending. Earlier entries below are historical.

> **v5.44 current:** Forward side-inspection spacing is 200 mm. After finding tape, a 30 mm probe tests whether it continues ahead. If it crosses off the tape, the car advances its axle to the recorded tape point, sweeps for the tape heading, confirms forward clearance, then follows. Missing tape/blocked paths remain bounded stops. Physical validation pending; see the v5.44 development-status entry. Earlier entries below are historical.

> **v5.43 current:** Tape encountered after leaving the original line can interrupt the detour, including the return leg. The car stops, checks forward clearance, then tries following; it no longer always advances the axle and turns to the original heading. Unknown/blocked candidates resume the detour. Cautious turns have bounded starting-torque assistance; straight legs retain their slow profile. Physical bottle-on-bend test pending. See the v5.43 development-status entry; earlier versions below are historical.

> **v5.42 supersedes v5.41:** Wi-Fi now runs on the second core; the motor guard remains 150 ms. Cautious bypass right turns now target 1045 counts/wheel for the reported 85-degree observation. Left-turn targets and the v5.41 route/speed settings below remain. Physical bypass completion is still pending.

> Current test build: **v5.41**. The original v5.35 design below is retained as history. v5.41 uses a nominal 200 mm initial offset, 100 mm extension/along steps, 600 counts/s cautious straight/turn control capped at 24% PWM, minimum 400 mm along travel, and a 240-second episode bound. Two clear side checkpoints must include valid far central-ray echoes; all-no-echo scans cannot authorize return. Standalone calibrated turns keep their original profile. User-reported chassis width is 11–12 cm, rear overhang about 5 cm. These settings still require a physical clearance/turn test. See `docs/development-status.md` for evidence and verification.

# v5.35 obstacle bypass — first physical test

1 November 2026. User requested pausing half-barcode work and testing object bypass.
This is a new **supervised calibration mode**, not physical acceptance or the final RTOS mission.

## Run it

- Flash `build/motion-wireless/robocar_motion_integration_v5_35_wireless.uf2`.
- Join AAI10-Calibrate; open `http://192.168.4.1/` and refresh until v5.35 appears.
- Choose **Follow track + avoid obstacle**, Prepare run, then GP20.
- Keep GP28 centred on black tape and the car still during the three-second countdown.
- Use a straight tape section on the floor, with an upright bottle about 50–60 cm ahead of the sonar. Leave approximately 1 metre of clear floor to the right and at least 1 metre of straight tape after the object. Move nearby laptop/chair legs away from the scan area. A short tabletop cannot accommodate this bypass; there is no edge detector.
- Expect pauses and chassis turns to look towards the bottle. They are intentional. GP20 stops locally; browser Stop requires Wi-Fi.
- Once it returns and follows the tape, stop with GP20 and Download log. Report contact, clearance and whether the returning car was aligned with the tape. Do not lift it while it is still driving.

## Behaviour

Barcodes are disabled in this mode. Existing course and diagnostic modes remain available.

1. Follow with the existing GP28-centred controller. Two fresh front returns at or below 30 cm start a bypass.
2. Stop and settle; turn right 90 degrees using the existing calibrated encoder targets.
3. Scan the outward path at 50/70/90/110/125 degrees, three fresh readings per angle after 300 ms settling. A blocked ray vetoes that path.
4. Move outward nominally 300 mm; face the original forward direction and scan again. If still blocked, extend outward by 150 mm, up to a 600 mm total offset. A blocked outward corridor or the offset limit stops the run. No automatic left bypass or reverse is attempted.
5. Move alongside in nominal 150 mm steps. Between steps, stop, turn left 90 degrees, scan towards the object, then turn right again to continue. This uses the tested servo envelope rather than commanding an untested sideways servo angle.
6. Return is allowed only after an object has been observed in a side scan, two later side checkpoints are open, and at least 600 mm of nominal forward bypass travel has occurred. The limit is 1200 mm; failing to observe/clear the object stops the run. These scans adapt the forward distance to the obstacle; they are not a full 3-D size estimate.
7. Approach the tape from its right, watching GP28. Accept a strong tape reading only within a bounded window around the expected return distance; stop and confirm it. Advance the axle using the reported 120 mm sensor lead, allowance for half a tape width, and measured coasting. Turn right 90 degrees to the original heading. Reuse bounded line recovery if needed and require stable line following before `[AVOID COMPLETE]`.

New straight legs use nominal 2543 counts/revolution and 60 mm wheel diameter, with a 900 counts/s target. These distances are **provisional**: the old 30 cm test has a special empirical endpoint that is intentionally not treated as a universal scale. Calibrated left/right 90 degree targets are unchanged. Repeated turn errors and wheel slip can affect the return position.

## Limits and failure handling

- Right-hand bypass on a straight section; not arbitrary-shaped obstacle navigation, sharp-bend avoidance, junction recognition or cliff sensing. Barcodes still must not lie under GP28's return path. The first-black-strip/expected-distance gate cannot prove the identity of a marking.
- No echo is logged separately from a valid distance. Three no-echo samples at a ray contribute provisional out-of-range evidence, not guaranteed clearance. Return additionally requires prior side detection and two separate open checkpoints. A disconnected/stuck-low sensor cannot be distinguished from open space by this hardware alone. Echo high before trigger, stuck high after trigger, and implausibly short pulses stop the new mode.
- During straight bypass legs, two front readings at or below 20 cm stop; one at or below 12 cm stops immediately. No recursive avoidance during a bypass. The sonar does not cover the full swept chassis footprint during rotation: leave the rotation area clear.
- Encoder invalid transitions, wrong direction, one second without wheel progress, missing/late control or ADC updates, stale sonar polling, travel bounds, and a 180-second bypass deadline stop the run. At most five bypass episodes per run; whole-run wheel-count bounds also remain.
- Servo commands stay within the documented 50–125 degree scan range; motor PWM slices remain separate. Each step yields to the main loop; GP20/browser Stop, the motor service watchdog, and Wi-Fi servicing remain active. Existing sonar acquisition is still a bounded synchronous wait (up to approximately 60 ms), not a completed RTOS driver.
- Summary `[AVOID RESULT]` is retained with the same Download log button; phase changes and per-scan evidence appear in the live log. A complete message means software confirmed GP28 alignment, not independent proof of safe physical clearance.

## Reference comparison

Reviewed Gerald's [obstacle controller](https://github.com/Gerald-codes/Line-Following-Robot/blob/main/src/control_algo/obstacle_control.c) and [avoidance manoeuvre](https://github.com/Gerald-codes/Line-Following-Robot/blob/main/src/control_algo/avoidance_maneuver.c). Its staged avoidance idea is relevant, but it uses IMU heading, estimated timed travel and timeout fallbacks (including a search timeout that marks completion). This implementation is independent code, uses this car's encoder turns, and requires a tape observation before completion. It does not copy the other robot's pins, motor polarity or yaw assumptions.

## Verification

- Sanitized obstacle-controller simulations: completed bypass, lateral extension, blocked outward/forward routes, object never observed, object not cleared, insufficient clear checkpoints, lost tape, early false markings, unstable tape, failed alignment, encoder invalid/stall, stale range, delayed loop, timeout and distance bounds.
- Production main-loop simulations include new-mode selection, no barcode capture, obstacle handling, GP20/browser cancellation, stopped PWM, servo recenter and retained summary; existing centred and legacy course scenarios are also run.
- Existing barcode/motion and wireless core/HTTP regression suites; browser test of actual HTML against a mocked car, including the new selector, progress messages, Stop and downloads.
- Pico W release build with warnings treated as errors and UF2 family/block/address/version checks.

None of these tests operates the physical robot. Tyre grip, actual lateral distance, bottle echoes, chassis clearance, cumulative turn accuracy and final tape alignment remain the purpose of the first user run.
