## v5.56 tape alignment after straight seeking — 4 November 2026

The 09:09 log shows a qualified single tape crossing and axle advance,
followed by failed alignment sweeps. Straight seeking was still using A
to restrict tape acceptance: A=65 allowed only 45–90 degrees, although
strong tape was logged near 110 degrees. Straight-seek alignment now accepts
candidates in the bounded 25–115 degree sweep and pauses immediately for
stationary confirmation instead of requiring 80 ms of black while rotating.
Transient ink still resumes the sweep. This does not add any sonar sweeps
or change the two scan stages. Physical axle placement, turn calibration
and the separate right-wheel stall in the log remain to be checked.

### Two scan stages

Only the initial 5-degree edge scan and the forward-facing check after an
angled 100 mm move command servo sweeps. After calculating A, turn right A
and move 100 mm directly; then turn left A to restore the original heading
and scan. A blocked check repeats this loop. Three completed centre
no-returns (or the existing two valid far centre readings), with no valid
blocked ray, start straight tape seeking. During straight seeking, keep the
servo centred and poll sonar normally; do not sweep again. Tape qualification,
live obstacle and encoder guards, and bounded straight travel remain active.
An inconclusive initial edge stops rather than invoking the old lateral
shift/side-scan fallback. No outward clearance sweep or automatic return turn
is part of this normal sequence. Physical retest remains required.

### Earlier straight seek behaviour

After each checked angled step, turn left by A to restore the original
heading and scan forward. If blocked, turn right by A and repeat a checked
angled step. If clear, continue straight at the original heading in checked
100 mm legs, looking for and verifying tape. Three completed centre
no-returns or two valid far centre readings establish the forward clearance
condition, subject to any valid blocked ray. The normal path no longer
commands a left return turn or requires 250 mm lateral offset. Straight
seeking remains bounded by the 1200 mm along-travel limit and the existing
obstacle, sensing and encoder guards.

### Earlier step check behaviour

After each completed angled move of at most 100 mm, the car turns left by A
once to face its original heading and performs a forward-facing scan. A
detected object causes a right turn by the same A, one checked move of at
most 100 mm, and another return to the original heading for a new scan. A clear scan (two
valid far centre readings or three completed centre no-returns, with no
blocked ray) causes a left turn by A toward the tape. The car scans that
return path, moves while monitoring the sonar, and verifies black tape
before resuming line following. No second left turn or two side-clear scans
are required in this normal path. Even after a clear forward scan, the car
needs at least 250 mm lateral offset before it can turn toward tape; the
front-mounted line sensor and body would otherwise remain too close to the
object and original tape. The car cannot reach straight tape by
travelling parallel to it at its original heading from a lateral offset.
Encoder turns and chassis clearance still need a supervised physical test.

### Earlier three no-return rule

The 07:55 run exposed an inconsistent rule: three completed sonar
no-returns could propose an obstacle edge or confirm the object was no longer
at the side, but the outward path scan still demanded two valid far echoes.
The path scans now accept either two valid far centre readings or three
completed centre no-returns. A valid close reading still blocks the path.
While moving, completed no-returns do not count as sensor-invalid readings;
the car remains limited to checked moves of at most 100 mm and stops on valid
close readings, stale sensing, sonar faults, or three unqualified invalid
readings. This follows the agreed three no-return interpretation, which is
not independent physical proof of a clear floor. Test with space around the
car and record each checked move and body clearance.

### Earlier original heading check behaviour

The 06:24 run completed an angled outward move at A=70 degrees. One run then
stopped at `SCAN_FORWARD`: the centre sonar had no valid distance, so the car
could not safely start the old parallel leg. The new sequence finishes its
checked angled departure, turns left by A to face the original track
direction, then turns left 90 degrees to aim the forward sonar at the object's
side. The servo alone reaches only 35 degrees left of the chassis, so that
extra chassis turn is required for a direct side check. If the object remains,
the car restores the original heading, turns right by A, scans the outward
path and moves at most 100 mm. It repeats this while the object remains,
subject to the 600 mm lateral limit. Two clear stopped side scans after an
observed object start the existing scanned, monitored return toward the
tape. Completed centre no-returns can support the side check, but invalid
readings or no-returns never authorise motion without the return-path scan.
The car must turn toward the tape to cross it: travelling in the original
heading while offset would run parallel to the tape. Observe each turn and
body clearance during a supervised test; encoder turns and sonar beam width
remain uncalibrated.

### Earlier side angle development behaviour

The 06:07 run found the right edge and planned A=60 degrees. After turning,
the centre path had valid readings near 110 cm, but the 125-degree oblique
ray measured 27–29 cm and fell just inside the provisional body corridor.
The controller stopped with zero outward travel. An isolated blockage on
that ray now turns the car another 10 degrees right and repeats the stopped
path scan. This is allowed at most twice, before outward travel only, and
only when the centre has at least two valid clear readings and there is no
close or other blocked ray. Thus A=60 may become 70 or 80 degrees; the
initial 5-degree edge margin is 15 or 25 degrees after adjustment. A blocked
centre, a very close echo, or uncertain clearance still stops the car. The
short turn and physical body clearance need a supervised car test.

### Earlier echo edge behaviour

The stopped initial scan now samples 90 to 50 servo degrees in 5 degree steps.
Two valid near readings followed by two valid far readings at the next
rightward angle locate the right edge. Three completed no-return readings at
that next angle can instead propose a candidate edge. A candidate permits a
turn and stopped path scan, but movement still needs two valid clear centre
readings and no blocked ray. Candidate edges also require an observed object
on the side before the car can return to the line. The calculated angle A
includes the body clearance, beam uncertainty and an additional 5 degrees.
Other no echoes remain unknown. If the edge remains unbounded, the car turns right, checks
the path, moves 60 mm, turns left and scans again. It can repeat this up to
300 mm of lateral shift. If the obstacle disappears from the forward scan,
the car checks its left side before travelling alongside. Unconfirmed paths
stop the run. Once an edge is found, the car checks the outward path before
each move of at most 100 mm, turns left by A to travel
parallel, and uses the inverse of A after repeated side clearance to seek tape.
In the earlier build, outward, forward and return scans required valid centre clearance before moving.

For the first supervised run, use a single narrow object on a straight tape
section and leave ample floor space on both sides. Record the initial scan,
actual turn angle, each outward step, closest body clearance, side checks and
return turn. Download the log; `[AVOID PLAN]` and `[AVOID RESULT]` show the
confirmed edge and A. These dimensions are provisional until the car is tested.

## v5.56 retest — 3 November 2026

Flash `firmware/releases/robocar_motion_integration_v5_56_wireless.uf2`, refresh
http://192.168.4.1/ and confirm v5.56. Select **Follow track + avoid obstacle**,
prepare the run, then GP20. Keep the existing GP28-centred/GP27-right mounting.

1. Straight-strip bottle: expect a stopped width scan, a calculated right exit
   (or90degrees if the edge is unknown), a diagonal move, and a turn parallel to
   the original route. Observe actual clearance; calculations are provisional.
2. The car checks left between parallel legs, may add10cm rear margin, then
   turns to scan towards the object. It must verify clearance before returning.
   Confirm125degrees physically points left; a servo mounting reversal changes
   the scan geometry and must be corrected before interpreting the planner.
3. At perpendicular tape rejoin, it selects right first. It must resume following
   without hitting the bottle. Repeat with the bottle on a bend.
4. On a clear straight, check the faster cruise. Through a bend, check continuous
   correction and no repeated departure. Unknown direction still triggers search.
5. Download one combined log and record video for each setup. AVOID PLAN gives
   angle, lateral/diagonal/forward distances, observed span and edge confidence;
   AVOID LEFT records the oblique check. Report contact or wrong-direction rejoin
   even if it later completes. GP20 is the local stop.

This is a new physical-test build. Scan estimates cannot guarantee exact width,
object depth or empty space, and faster cruise remains unverified on the foam.

## v5.55 retest — 3 November 2026

Flash `firmware/releases/robocar_motion_integration_v5_55_wireless.uf2`, refresh
http://192.168.4.1/ and confirm v5.55. Use **Follow track + avoid obstacle**.
Keep the current GP28-centred / GP27-right mounting.

1. Bottle on a straight strip: check the 25 cm initial detour, passing the
   bottle, rejoining in the original direction, and continued following.
2. Bottle on the bend: check that it rejoins the route, rather than a barcode
   or printed sign, and continues beyond the bend without returning to the bottle.
3. Save each log and video. The log should show SPATIAL_INK_CHECK followed by
   SINGLE_CROSSING or CONTINUOUS_INK for accepted tape. INK_TOO_NARROW or
   REPEATED_INK means the candidate was rejected and the original detour resumed.
   A real tape rejection is a failed test, not proof the track is wrong.

The 60 mm ink check and 120-degree approach-heading bound are provisional.
Very narrow tape, printed strokes, tight geometry or wheel slip can defeat
these heuristics. A persistent wheel jam still stops after one second; endpoint
assistance does not disable that guard. Record any contact or wrong-direction
rejoin even if the run later completes. Physical acceptance is still pending.

## v5.54 retest — 3 November 2026

Flash `firmware/releases/robocar_motion_integration_v5_54_wireless.uf2` and
refresh the page to v5.54. Keep GP28 centred over tape, GP27 to its right and
GP26 for barcodes. Select **Follow track + avoid obstacle**, prepare, then GP20.

Repeat bottle-on-bend and bottle-on-straight tests. After rejoining, allow it
to continue into the next left bend. Record whether it keeps following and
download the log before switching off. Look for `[AVOID COMPLETE]`, subsequent
following/search states, and the final stop reason. v5.54 is not yet physically
validated. Rejoin now uses 450 cps and a wider bounded correction; normal cruise
speed and the 25 cm initial detour remain unchanged. It still stops on exhausted
recovery or faults; it does not implement a new symmetric two-sensor layout.

## v5.53 — found tape with no echo; shorter initial detour

Use the v5.53 release UF2. The initial lateral encoder target is 25 cm; measure its actual foam travel if practical. Once the car finds stable tape, two completed no-return readings can now permit the monitored probe/axle-advance/right-first alignment sequence. It no longer rejects that tape merely because no distant object reflects ultrasound. A missing ECHO rising edge, stale data or sensor fault does not qualify. Close-object stops remain. Slow turns can apply up to 60% drive under load with feedback; physical turn precision and clearance still need testing.

Run the same **Follow track + avoid obstacle** mode on (1) a straight-strip bottle and (2) a bend bottle. Observe whether it returns to following after finding tape, and download before switching off. The latest and preceding stop/detail snapshots remain available within the same boot. New `[AVOID TAPE]` and `tape_decision` fields explain each rejoin choice. Software and simulated geometry checks pass; v5.53 physical acceptance is pending.

## v5.52 — foam turn effort and failure retention

Flash `robocar_motion_integration_v5_52_wireless.uf2`, reload the page and verify v5.52. Select **Follow track + avoid obstacle**, prepare, then press GP20. Test a bottle on the straight strip before repeating the bend. Observe completion of the initial right and subsequent left turns and resumed line following. Download before power-off: current and preceding run stop/detail snapshots are retained, even if normal log text wraps. New turn effort is capped at 45% with speed feedback; endpoints and the one-second hard-stall stop remain. Simulation and build checks pass; physical success is unverified.

> **v5.51 local test:** Adds a brief forward bend arc after line loss when recent direction evidence exists. Maximum 450 ms / ~20 mm wheel travel / ~8 degrees encoder-estimated yaw; otherwise stopped recovery. Strong-tape confirmation resumes following without a stopped launch. Unknown direction, recent barcode activity, crossings, near-obstacle approach and obstacle rejoin keep the existing recovery. v5.50 obstacle settings retained. First test clear bends with the bottle removed; compare video and BendOK/BendFallback logs. Physical improvement remains pending.

> **v5.50 local test:** 20 cm bypass target; 45 cm only slows straight approach. A remembered target and encoder travel prevent missing/background echoes from allowing unlimited approach; that conservative estimate may stop earlier than an actual 20 cm sonar reading. 30 cm lateral offset retained. Turn PI retains loaded effort up to 32% and holds a wheel that leads by >120 counts. Flash v5.50 and test straight foam first, then bend; record approach clearance, both wheels during the turn, and continued tape following. Software/build checks passed; physical validation pending. Earlier entries are historical.

> **v5.49 local test:** First valid reading within 45 cm stops approach immediately; missing/background echoes retain the warning. Initial side offset is now 30 cm. A reading below 25 cm before the first pivot stops with AVOID_APPROACH_TOO_CLOSE. Flash v5.49, start the bottle about 50–60 cm ahead of the sonar, test straight then bend, and record contacts plus resumed following. Leave ample clear floor for the wider detour. Software/build checks passed; physical validation pending. Earlier entries below are historical.

> **v5.48 local test:** Side clearance can use repeated far echoes across two scan angles when the central echo is absent. Confirmation repeats while stationary, with at least 200 mm travel since the last side-obstacle observation and 400 mm total along travel. The outward scan accounts for the car corridor; bypass-only turn trims are provisional (R 1100 / L 1090 counts). Flash v5.48, test the bottle on a straight strip first, then the bend. Physical return and angle validation remain pending; prior versions below are historical.

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
