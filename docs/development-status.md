## 3 November 2026 — v5.53 school-track rejoin and loaded-turn fixes

Publication checkpoint: source, tests and progress notes since v5.47 are packaged on `yixuan/obstacle-rejoin-v5-53`, with the v5.53 UF2 in `firmware/releases/`. Earlier entries' unpublished status describes their state when written. This checkpoint does not claim a successful v5.53 physical run or final-course acceptance.

Evidence from v5.52: `AAI10-motion-2026-11-03T03-03-08-616Z.txt` and `AAI10-motion-2026-11-03T03-04-40-983Z.txt`. These downloads overlap; the latter contains the preceding run, so repeated results are not extra tests. The first run detected tape during MOVE_ALONG and held it: raw GP28 stayed about 2542–2580 and GP27 about 1335–1350 during CHECK_FOUND_TAPE. All sonar observations were completed NO_RETURN_WITHIN_WINDOW. The two-valid-far-echo condition never passed; after 1.5 seconds the controller rejected the tape and resumed driving across it. It later stopped in FACE_ALONG: right wheel 1057/1100 counts, no progress 1040 ms, applied PWM -0.450; left had finished at 1121 counts. The second stopped in FACE_OBJECT: right 434/1090 counts, no progress 1032 ms at +0.450, left -598/-1090 and held to prevent excess lead. Both had zero invalid encoder transitions. The confirmed observation is right encoder non-progress under commanded drive; friction/load is an addressed hypothesis, not proof that electrical or mechanical faults are absent.

Changes:
- Stable found tape can enter the existing probe/axle-advance/right-first alignment sequence after two fresh, settled, completed no-return/over-range measurements with no sensor fault. This is permission for a bounded monitored rejoin, not a claim of proven empty space. NO_RISE, stale readings, explicit sensor faults and close obstacles cannot grant it. Alignment confirmation uses the same rule. Persistent lack of usable observations while tape remains present now stops at that tape instead of discarding it and driving across. Side-scan obstacle-clearance rules retain their separate evidence requirements.
- Slow turns retain their encoder targets and 450/600 cps speed demands but can build to 60% effort, with 2750 integral headroom, 1.2 duty/second acceleration ramp and 250 ms no-progress breakaway request. Leading-wheel holds remain; one-second stall and 12-second turn limits remain. No extra angle is hardcoded to disguise a stalled wheel.
- Initial lateral target reduced from 300 to 250 mm at the operator's request. Actual foam travel is not yet measured. Later extensions are clipped to the existing 600 mm overall limit, including the final 50 mm step from 550 mm. Close-object and corridor scans remain.
- TAPE transition diagnostics explain acceptance/rejection; the retained MOTION DETAIL includes the last tape decision. Startup metadata now reports the actual 60% turn cap and 250 mm target. Retained previous/current run reports and download retry fixes from v5.52 are included.

Validation: sanitized regression passes for the exact persistent-black/no-return rejection condition and full perpendicular rejoin under completed no-return readings. Missing rise, absent freshness, sensor fault and close-object cases do not authorize rejoin. Both turn directions and either loaded wheel are simulated with resistance starting halfway through or near the endpoint: an emulated old 45% ceiling stalls, new bounded effort completes. Hard jams still stop. All 21 obstacle scenarios, 11 rejoin geometry cases, 45 production-main cases, centre-bend, standalone motion, wireless core/HTTP/concurrent-radio and isolated browser tests pass. Strict Pico W build and UF2/embedded HTML verification pass. Tests model logic, not tyre slip or actual motor torque.

Next physical tests: flash v5.53, use Follow track + avoid obstacle, repeat straight-strip bottle then bend bottle. Check lateral travel, both turn directions, right-first alignment after tape detection and resumed following. Save the log before power-off. Remaining work includes reliable real-course obstacle acceptance, barcode reliability, final mission integration and the µT-Kernel port.

Artifact: `firmware/releases/robocar_motion_integration_v5_53_wireless.uf2`; 982528 bytes; SHA-256 `2e704d6f7ea163050b2eb8defb7da1d8d216012b06d295c3f3197499628f374e`.

## 3 November 2026 — v5.52 loaded turns and retained wheel-failure evidence

The v5.51 log `AAI10-motion-2026-11-03T02-27-32-932Z.txt` retains NO_ENCODER_PROGRESS_1S in FACE_FORWARD for the first run and TURN_RIGHT_OUT for the second. Videos `20261103_101427.mp4` and `20261103_101536.mp4` show an incomplete return-facing turn and incomplete initial right turn respectively. The rolling log lost pre-stop movement and result retrieval failed; the evidence does not identify the stalled wheel or prove its physical cause. Insufficient loaded turning effort is the hypothesis addressed here, not a proven hardware diagnosis.

Bypass turns now allow speed-feedback drive up to 45% rather than 32%, with integral headroom 2000, 0.8 duty/second acceleration ramp and a 250 ms no-progress breakaway request. Existing 450/600 counts/second demands, turn endpoints, 120-count leading-wheel hold, one-second no-progress stop and 12-second timeout remain. Held turn wheels retain learned integral effort with the held tick's integration undone, rather than discarding it on every hold. Alignment advance retains its previous 32% limit; ordinary line driving, 20 cm approach and v5.51 bounded bend continuation are unchanged.

Both wheel updates execute before evaluating a stall, capturing left/right/both failure bits, no-progress time, last applied PWM, requested/measured speed, targets, positions, held/finished states and integral. MOTION DETAIL also records obstacle phase and sensor/encoder context. The wireless result stores this outside the rolling log and retains the preceding run's stop/detail when a new run starts. HTTP capacity is checked at compile time for all eleven maximum-length retained lines. The browser appends recovered snapshots to its download history, retries a failed idle recovery, and preserves its last successful snapshot through a later Wi-Fi failure. Power cycling still clears device retention.

Validation: sanitized obstacle tests reproduce failure with an emulated old 32% limit against a 35% loaded-wheel deadband, and complete left/right turns with either loaded wheel using the new control. Both-wheel hard stall captures mask=3 and 1000 ms ages with nonzero pre-stop PWM and zero final outputs. Existing obstacle/rejoin, centre-bend, standalone motion and all 45 production-main scenarios pass. Wireless ring-overwrite retention, previous-run preservation, eleven-line HTTP response, and concurrent-radio tests pass. Mock-browser tests pass including offline cached-result preservation. Strict Pico W build, exact embedded HTML, firmware markers and every UF2 block verified. No physical flash or run by the agent; foam success remains to be tested.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_52_wireless.uf2`; 980480 bytes, 1915 blocks; SHA-256 `69509e3c716f602dc4b041c6380651f29ec35e9a7b5fe76a378065d0c8936aad`. ELF BSS 133140 bytes. Local only; not pushed or merged. Next tests: straight-strip bottle bypass, then bottle on bend, using Follow track + avoid obstacle.

## 3 November 2026 — v5.51 bounded rolling bend continuation

User requested permitting a little line overshoot and steering toward its last known side to reduce stop/search pauses. Implemented in the centred course controller used by course and obstacle-follow modes, retaining v5.50's 20 cm approach, loaded turn control and 30 cm lateral bypass offset. No v5.50 physical result has been received yet; the new bend behaviour is also a test build, not physical acceptance.

A rolling continuation requires prior strong tape while moving for at least 300 ms/200 counts per wheel and a directional hint no older than 500 ms. GP27 on black while GP28 is weak supplies a right hint. A left/right trim becomes a hint only after measured directed yaw and an optical improvement; reversed/exhausted trials do not qualify. GP28 alone cannot tell which side the line was lost on. Unknown direction keeps the existing stopped search.

On loss with qualifying evidence, retain wheel-speed feedback and move in a forward arc: 220 cps differential demand for the first 40 ms, then up to 420 cps with a 600 cps outer target. Strong tape tapers the differential to 100 cps; 40 ms of strong tape confirms a rolling rejoin without resetting drive into a stopped launch. Maximum continuation is 450 ms, 270 counts (~20 mm) per wheel, or 180 differential counts (~8 degrees). Wrong direction, explicit disablement and budget exhaustion brake into the existing bounded search. Weak/brief ink cannot renew the budget; another attempt needs strong moving-tape evidence again. Recent barcode activity, crossings, near-object warning and obstacle VERIFY rejoin do not permit this excursion. Independent barcode acquisition retains its own existing policy; the legacy course read-permission predicate excludes the rolling arc.

Cruising speed is unchanged. The intended time saving is fewer brake/search/relaunch pauses. The 2 cm limit is wheel travel, not a guaranteed lateral sensor clearance; sensor lead and tyre slip still matter. This cannot guarantee recovery from arbitrary sharp bends or identify every adjacent black marking. Live logs expose BendActive/BendDir/BendTries/BendOK/BendFallback and final BEND RESULT; counters are for the current course segment.

Validation: `python3 tools/test_center_bend.py` passes left/right rolling rejoin, stale/unknown hint, startup, barcode, crossing and near-obstacle disablement, no rearming on flicker, time/distance/yaw/reverse limits, late control and encoder faults; existing centre steering tests also pass. Sanitized obstacle suite (21 scenarios plus geometry/approach/drive cases), 45 centred production-main scenarios, motion host, barcode drive, and wireless core/transport/concurrent-radio tests pass. Strict Pico W build and full UF2 structure, diagnostics and embedded HTML validation pass. No flashing/motor commands by agent. Next: clear-track bends first, recording video/log, followed by obstacle tests after confirming tracking.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_51_wireless.uf2`; 975360 bytes; SHA-256 `1b5c4b73c876a7d6a4e1e61d76deffa950338a0aa764f87778ba09af329b6707`. Local changes, not pushed or merged.

## 3 November 2026 — v5.50 20 cm approach and loaded turn control

User requested restoring a 20 cm obstacle trigger after v5.49 turned out at 43–44 cm. School videos `20261103_092021.mp4` and `20261103_092206.mp4` show partial first right turns, before any lateral bypass. Second log `AAI10-motion-2026-11-03T01-22-57-016Z.txt` shows approximately 1100 left counts versus 450 right counts, with repeated right-wheel near-zero progress despite drive. The first download ends at MOTION GO; the second ends mid-turn and both lack a recovered result. Exact final stall/timeout cannot be established. Foam load is a plausible explanation for the physical difference; encoder counts alone do not establish its mechanical cause.

Approach change: 45 cm now starts a warning and limits straight approach to 450 cps; it does not initiate a bypass. The existing steering effort remains available for corrections. First fresh valid <=20 cm brakes immediately; two near samples or 600 ms stopped confirmation starts the bypass. A remembered near target also supplies a conservative remaining-distance budget based on encoder translation and front-tip rotation from a fixed observation origin, with 20% margin. Farther/background and missing echoes cannot erase that target. Budget <=20 cm requests the same stopped confirmation; it is an estimate, not a measured sonar range, so the physical stop can be earlier. Net heading change >0.35 rad during approach stops with AVOID_APPROACH_HEADING_UNCERTAIN instead of assuming the remembered target is still ahead. Readings <12 cm before the initial pivot stop with AVOID_APPROACH_TOO_CLOSE. This replaces the old 25 cm pivot refusal so a 20 cm approach is usable; exact sonar-to-tip geometry and physical clearance remain unvalidated. Existing 300 mm lateral offset and side/return checks are retained.

Turn change: slow-turn PI can retain up to 32% drive with increased integral headroom, rather than dropping back below useful effort after every small movement. Turn-only breakaway assist is capped at 32%, and direction-normalised wheel progress holds the leading wheel when it is >120 counts ahead. The lagging wheel retains its original stall timer. Speed targets, turn counts (R1100/L1090), acceleration ramp, one-second hard-stall stop and 12-second turn timeout remain. This is a load-control change, not an angle increase.

Validation: sanitized regressions cover stationary 43 cm warning without bypass, 20 cm first-sample braking, near-target loss/background with bounded encoder travel, too-close and heading guards, asymmetric right load in both turn directions, sustained effort, bounded wheel lead and a jammed wheel. Existing 21 obstacle scenarios and perpendicular rejoin cases pass. All 45 centred production-main scenarios, standalone motion, centre course, barcode drive, and wireless core/transport/concurrent-radio tests pass. Strict Pico W build and UF2 structure/version/embedded HTML checks pass. No physical flash or run by agent. Next user test: bottle on straight foam then bend, capturing the full approach, first turn, return and continued following.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_50_wireless.uf2`; 971776 bytes; SHA-256 `ec5c97b60133b91bf8292b570305c1ff9b9a92d3d3d30b172346348f4e689c47`. Local, not pushed or merged.

## 2 November 2026 — v5.49 earlier obstacle braking and front-tip clearance

Evidence: latest runs in `AAI10-motion-2026-11-02T09-23-08-149Z.txt` and `09-26-41-596Z`, plus one-second frames of `20261102_172359.mp4` (83.327 seconds). Both runs emitted a completed bypass and resumed following. The straight-run video shows contact around the approach and side inspection; log gaps prevent exact attribution of every intermediate transition. Before the first turn, sonar reported the bottle at 41.2 down to 34.2 cm, then NO_ECHO and distant background readings, before finally stopping at about 12 cm. The old 30 cm trigger missed the earlier useful observations. A separate contact during side inspection exposes insufficient room for the protruding sensor at the old 200 mm lateral offset.

User reconfirmed 11–12 cm total width and 13 cm axle-to-front-tip reach. A conservative rectangular front corner has an approximately 14.3 cm axle-centred swept radius. Exact sonar-face-to-tip distance was not supplied. This geometry motivates more clearance; it does not prove complete collision coverage from one sonar beam.

Change: the first fresh valid echo at or below 45 cm immediately requests zero motor output and retains an approach warning. Two near observations, or a 600 ms stopped confirmation interval, initiate bypass. Missing echoes and farther background cannot erase that warning. A closest observation below 25 cm, including while settling before the initial pivot, stops with AVOID_APPROACH_TOO_CLOSE instead of attempting a cramped turn. Initial sideways offset increases from 200 to 300 mm. Existing turn targets, side-clearance confirmation, tape rejoin, sonar/encoder and distance/time guards remain. No-echo alone without a preceding near observation does not invent an obstacle. Logs expose approach minimum, hit count and decision state; UI receives only the version change.

Validation: sanitized obstacle regressions include first-hit braking, near-to-no-echo/background retention, second-hit confirmation, too-close approach/settle rejection and open-space no-echo. Existing 21 obstacle scenarios, perpendicular geometry and adaptive-drive cases passed. All 45 centred production-main cases and wireless core/transport/concurrent-radio tests passed. Strict Pico W build passed. All UF2 headers, family, block addresses, version, diagnostics and exact embedded HTML verified. No physical flash/test by agent. Next: straight bottle bypass followed by bend bypass, with enough floor space for the wider detour. A single sonar beam cannot guarantee detection of every object or full swept-volume clearance.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_49_wireless.uf2`; 970752 bytes; SHA-256 `9c61c59feb0d9edbb689abfae807224c179228ba7ae8b7a6093063c858217584`. Local changes on `yixuan/obstacle-clearance-v5-48`; not pushed or merged.

## 2 November 2026 — v5.48 side-clearance confirmation and bypass turns

Evidence: reviewed the latest run in each of `AAI10-motion-2026-11-02T08-49-37-350Z.txt`, `08-50-58-199Z`, `08-54-51-985Z` and `09-00-26-979Z`, plus one-second frames of `20261102_165826.mp4` (88.471 seconds). Downloads contain old/duplicate runs and log gaps. First run recorded one completed bend bypass, then CENTER_LINE_NOT_REACQUIRED; this does not establish a complete lap. Second stopped AVOID_RIGHT_PATH_BLOCKED: nearest 34.4 cm, with nearby returns at servo 50 degrees and centre readings around 89 cm. The reflector's identity is not established. Third/fourth saw the bottle alongside, then valid distant side returns (for example 74.3, 113.2 and 130.3 cm) but no central echo; scan_center_clear stayed zero and the old rule kept extending the bypass. Fourth later found tape after 600 mm of completed along legs and a partial leg, advanced the axle and stopped AVOID_ALIGN_OBSTACLE_STOP on a 361 us echo (about 6.2 cm). The video supports repeated inspections after passing the bottle; it does not prove what produced the final close echo.

Change: after observing the object, require at least 400 mm total along travel and 200 mm since the latest blocked side scan. A clear scan may now use two distinct angles with at least two valid far echoes each, or the existing two valid central echoes. Any close side ray vetoes clearance. Repeat the complete scan while stopped at the same position; two qualifying scans permit the existing cautious monitored return. Missing echoes alone, isolated far readings and only one noncentral far ray do not qualify. This is a guarded return decision, not proof that the entire swept volume is empty. Forward collision, stale data, distance/time, motor and tape guards remain.

The outward-path scan projects the echo bearing into a provisional 20 cm wide corridor (12 cm reported body width plus 4 cm each side), widening rays by 15 degrees for beam/heading uncertainty. A 34.4 cm echo at 50 degrees lies outside it; a central echo at that distance or any <=12 cm echo still blocks. This geometry must be checked on the chassis and does not classify the object that reflected the sound. Side and forward scans retain their existing close-ray vetoes.

Turn trims are provisional: bypass right targets 1100 counts/wheel (from 1045, +5.3%) and left 1090 (from 994, +9.7%). The video shows settled turns short of a right angle, with variation and perspective; these increments are conservative trial adjustments, not an exact angle measurement or closed-loop yaw control. Standalone motor calibration and the tape-guided alignment sweep are unchanged. Scan logs now include nearest/blocked angles, far-ray counts and last observed side checkpoint. UI version changes only.

Validation: sanitized 21 obstacle scenarios plus corridor regressions, ten perpendicular geometry cases, adaptive-speed/drive/encoder/sonar guards passed. A missing-central-echo/two-far-angle scenario completes at the 400 mm checkpoint; one-ray, isolated-echo, all-no-echo and interrupted-confirmation cases reject clearance. Production centred main and wireless core/transport/concurrent-radio tests passed. Strict Pico W build passed; UF2 family, sequence, addresses, version, diagnostics and embedded HTML verified. No physical run or flash by agent. Next: bottle on straight strip, then bend; capture the detour, full return, tape alignment and resumed following. Keep the test away from table edges; sonar is not cliff detection.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_48_wireless.uf2`; SHA-256 `0a77b3a48100b7f551607e53a6a16e8a18f08210c20ba6053cfdc8345fe467e7`. v5.47 remains the published main baseline; these fixes are on `yixuan/obstacle-clearance-v5-48` and are not yet pushed.

## 2 November 2026 — v5.47 conditional faster cruise and bypass transit

Physical evidence: `AAI10-motion-2026-11-02T08-27-59-635Z.txt` and video `20261102_161854.mp4` show one successful v5.46 bend-obstacle bypass. Alignment accepted at an encoder-estimated 100 degrees, no rejected candidates or reverse sweep; VERIFY_LINE completed its forward proof and emitted `[AVOID COMPLETE] count=1`. The robot continued round the tape and eventually stopped with CENTER_LINE_NOT_REACQUIRED at the open tape end visible in the video. Raw encoder invalid counters and sonar retry counts were zero. There are log gaps, so do not derive exhaustive event counts or precise phase durations from live rows. This route reacquired the bent tape during MOVE_ALONG (1442/1441 counts), before side inspections; it does not physically validate the full straight-strip rectangular return.

Change: in obstacle mode only, two fresh valid forward ranges above 60 cm allow a maximum 800 cps course cruise. GP28 must remain at least 90% of the fixed run reference for one second, GP27 must be white, startup complete, and no steering trim/crossing active. Requested cruise rises at 200 cps/s from 600 to 800. Weak tape, a right sensor hint, trim, recovery, crossing, near/unknown range or rejoin verification removes the boost. The shared drive accepts an optional centre cruise request; zero/default retains 600 cps. Lower target changes take effect immediately in software; physical deceleration still depends on feedback and traction. Other course/barcode modes retain the old target.

MOVE_OUT and MOVE_ALONG alone can target 800 cps with two valid >60 cm forward readings; their PWM cap is 28%. They return to 600 cps within 900 remaining encoder counts and 450 within 400. On loss of clearance permission the original 600/450 cps profile and 24% cap apply. The alignment profile, turns, tape search, scanning/settling, stop thresholds and full rectangular bypass logic are unchanged. No claim of arbitrary-bend capability: current sensors do not preview the upcoming curve. Live log adds CruiseCps and TransitFast; no new UI controls.

Validation: sanitized adaptive-speed tests cover stable acceleration, cap/ramp, early weak-tape slowdown, right hint, no echo, nearer range, trim and faster endpoint-bounded transit on a simulated motor plant. Existing 18 obstacle scenarios, 10 perpendicular geometry cases, isolated encoder/sonar/drive guards, 44 centred production-main cases, shared drive/course/recovery/barcode/motion tests and wireless core/transport/concurrent-radio tests passed. Strict Pico W build and UF2 family, blocks, version and embedded HTML verified. No physical run or flash by agent; higher-speed success and full straight-strip bypass remain pending.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_47_wireless.uf2`; SHA-256 `e4a109998344a6222e8ee952b2175d1e5c5468793711590ceee8fae9a6d6ea9c`. v5.46 remains available as the demonstrated bend-bypass baseline. Test 1: repeat bottle on bend and observe sustained following after rejoin. Test 2: bottle on a straight section, enough clear space on the right and tape extending beyond it; observe outward move, forward travel/side checks, return and following. Side clearance still requires observation and valid clearance returns; NO_ECHO alone cannot authorise return. Preserve video and Download log with the page open throughout.

## 1 November 2026 — v5.46 right-first perpendicular rejoin

Evidence: reviewed video `20261101_223809.mp4` (56.96 s). Around 35–40 s the chassis turns towards the bottle for a side inspection; around 44–52 s it approaches/crosses the tape perpendicularly and its front sensor assembly enters the barcode area; around 53–56 s it makes a partial left turn and stops. The previously analysed v5.45 log reported AVOID_ALIGNMENT_TAPE_UNSTABLE after successful axle advance. Video supports possible front-sensor confusion by barcode ink but cannot prove the identity of the marking under GP28 at every sample. The prior left/right choice used accumulated encoder mismatch, which does not establish route direction.

Change: a confirmed crossing now explicitly chooses right first. Alignment uses both wheel encoders relative to the axle-centred crossing, retaining that origin across the opposite sweep. A candidate must lie within a provisional 60–115 degree heading window in the selected direction and remain strong for 80 ms. Stopped confirmation requires continued tape presence through 300 ms settling and two fresh clear forward ranges. If tape weakens, resume the remaining sweep with its original target, rebase stopped velocity/stall timing and clear PI output; require white before accepting another candidate. Do not abort or restart the turn on a transient candidate. The existing 110 degree first sweep / 220 degree opposite fallback and episode limits still bound recovery. Close-obstacle, stale sensor, motor stall and travel guards remain. Earlier bottle inspection turns are unchanged. Barcodes remain disabled in avoidance mode; this is not an ink classifier.

Diagnostics: phase/result logs include alignment rejection count. `[AVOID ALIGN]` reports estimated heading, direction, rejection count, centre level and reference. UI/build version is v5.46; no extra controls added.

Validation: sanitized obstacle tests passed, including 10 perpendicular geometry cases (right-first despite prior wheel imbalance, early false ink, disappearance during confirmation then successful recovery, opposite sweep, missing tape, blocked route and loaded axle advance), existing 18 obstacle routes and guard/drive checks. Production-main centred tests and wireless core/protocol/concurrent-core checks passed. Strict Pico W build passed. UF2 family, block sequence, addresses, embedded HTML, version and diagnostics verified. Physical performance and tyre-slip/optical limits remain unverified. No flashing, serial session or robot operation performed.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_46_wireless.uf2`; SHA-256 `cf4e38954fca82ed0cede811d1253e858a0c420b4829128beced7762bb358e28`. Next test: same perpendicular rejoin, on a floor with room for the detour; record bottle approach through alignment and resumed following, and retain Download log. Expected first rejoin turn is right; opposite search only after exhausting the right sweep. A plausible ink pattern can still fool a reflectance sensor, so successful physical following must be observed.

## 1 November 2026 — v5.45 alignment-drive load recovery

Physical evidence: `AAI10-motion-2026-11-01T14-26-43-227Z.txt` contains two v5.44 runs, both ending NO_ENCODER_PROGRESS_1S in ADVANCE_AXLE_TO_TAPE. First: L=382/1605, R=368/1605; second: L=326/1592, R=295/1592. Encoder-based advance estimates are only 27.8 and 23.0 mm against nominal 119 and 118 mm targets. Both motors stopped registering movement while requested output reached approximately 24%. Invalid counts remained zero, sonar showed roughly 90–120 cm, and max main-loop times were about 53 ms. Neither run entered ALIGN_WITH_TAPE. Leaving black during axle advance was intentional and did not cause this halt. The physical resistance cause (friction, surface, snag, supply, etc.) is not established by these logs.

Scoped fix: PROBE_TAPE_HEADING and ADVANCE_AXLE_TO_TAPE now enable an alignment drive profile. Speed targets remain 600/450 counts/s. PI integral allowance increases from 700 to 1200 and output caps at 32%, allowing the controller to sustain the extra effort needed under load. A wheel without progress for 300 ms requests breakaway effort up to that cap, still ramping at 0.4 PWM/s. Fresh movement removes the special request; speed feedback can reduce output to zero on overspeed. No permanent minimum output is imposed. If one wheel leads by more than 120 counts, it is held while the lagging wheel catches up; only the deliberately held wheel's stall baseline is refreshed. The lagging wheel's one-second stall guard, motion endpoint, direction, invalid-count and timeout checks remain. Other straight detour legs retain their 24% cap, and turn assistance remains capped at 30%. Alignment-at-stop diagnostics expose target/progress, speed and PI values. No changes to course following, barcode decoding, inspection spacing or the tape alignment sequence.

Reference checked: Gerald-codes/Line-Following-Robot, `src/control_algo/avoidance_maneuver.c` (GitHub main retrieved this session). Its SEARCHING_LINE finds IR tape, CENTER_ON_LINE drives forward for 330 ms, then REALIGN_TO_HEADING uses its stored IMU heading. This supports separating tape detection, centring and alignment; it is not evidence of arbitrary-bend recovery on our chassis. No source copied. URL: https://github.com/Gerald-codes/Line-Following-Robot/blob/main/src/control_algo/avoidance_maneuver.c

Validation: a new load-response regression reproduces the old 24% stall after 300 counts and verifies completion with the alignment profile, unequal wheel loads, output/ramp bounds, overspeed response, and retained stops with one or both wheels blocked. The perpendicular geometric simulation now also reproduces the stalled axle advance with the old profile and completes advance/alignment/following under the same simulated resistance with the new one. Existing 18 obstacle routes, early-rejoin/turn/sonar/encoder checks, all 44 centred production-main cases, standalone motion tests, and wireless core/protocol/concurrent-core tests passed. Strict Pico W build and UF2 family/blocks/version/embedded-page checks passed. Physical acceptance remains pending; no flashing or robot operation. UI change is version text only; no browser interaction test performed.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_45_wireless.uf2`; SHA-256 `c281df82947f0246a030d61e37e4efdb458aadd619f7d0c4880e2e349f98249f`. Next supervised floor run should complete ADVANCE_AXLE_TO_TAPE and enter ALIGN_WITH_TAPE. If it still stops with both encoders stationary despite the extra effort, inspect actual wheel motion and underside resistance rather than extending the stall timeout or declaring alignment complete.

## 1 November 2026 — v5.44 perpendicular tape alignment and longer inspection spacing

Physical v5.43 log `AAI10-motion-2026-11-01T14-08-36-147Z.txt` ended with CENTER_LINE_NOT_REACQUIRED in VERIFY_LINE, completed=0, offset=200 mm, along=100 mm. GP28 briefly read black while driving, then the course follower searched left/right and exhausted its local recovery. Sonar remained valid around 98–111 cm during that search; invalid encoders were 0/0 and Wi-Fi max poll 3319 us. A LOG GAP obscures the precise tape-candidate transition; the retained result and visible VERIFY/search sequence establish the final failure. The user reports approaching the tape at approximately 90 degrees. Black observed after stopping does not make the prior recovery successful.

The previous 100 mm MOVE_ALONG endpoint was a hardcoded inspection interval, not a measured object end. v5.44 separates along inspection spacing (200 mm) from outward extension (100 mm). Front distance guards and early tape checks remain active during travel. The sonar mount remains within its verified 50–125 degree envelope, so chassis side-inspection turns are retained; this is not continuous sideways sonar tracking.

After a stopped tape candidate and forward clearance, a slow 30 mm straight probe distinguishes tape continuing ahead from a short crossing. A right-only candidate still stops the robot for checking but must also establish centre tape before committing to geometry. If GP28 drops below the existing white threshold during this probe, the car stops and advances the axle to the recorded tape position using 120 mm sensor lead plus an 8 mm tape allowance, minus probe travel and settling coast. A 70–140 mm remaining-distance bound rejects inconsistent geometry. Forward obstacle guards remain active during axle advance.

It then sweeps towards a heading closer to the pre-obstacle heading (encoder-derived preference, not a guaranteed route direction). The first sweep is bounded to 110 degrees; if no tape is seen, one reverse sweep of 220 degrees crosses to the other boundary. Strong GP28 tape stops the turn after at least about 14 degrees of sweep. Stopped tape confirmation and two valid forward readings above 30 cm are required before following. Unstable tape, missing tape, a close object or unconfirmed forward clearance stops with a distinct reason. Existing cautious turn assist, stall/direction/time limits and the post-rejoin forward-progress proof remain. Ordinary line following is unchanged. The direction of an unmarked line cannot be uniquely inferred from two IR sensors; successful physical reacquisition is still pending.

Validation: sanitized existing 18-route regression, early-rejoin and torque tests; a geometric differential-drive simulation with the sensor 120 mm ahead of the axle and a perpendicular tape verifies complete rejoin in both preferred/opposite sweep cases, missing tape, blocked axle advance and blocked new heading. This ideal model excludes tyre slip and real optical response. All 44 centred production-main cases and wireless core/transport/concurrent-core tests passed. Strict Pico W build and UF2 block/family/version/embedded-page checks passed. No robot flashed or operated, and no browser interaction test performed (UI changes are phase labels and version).

Artifact: `build/motion-wireless/robocar_motion_integration_v5_44_wireless.uf2`; SHA-256 `bfdf328c2ed156623e3ea8a067df4287c6d94852333b3498a1e215939d9e04ae`. Next physical test: same bottle on bend; check ADVANCE_AXLE_TO_TAPE, ALIGN_WITH_TAPE, CONFIRM_TAPE_HEADING, then following. Download the log after the run.

## 1 November 2026 — v5.43 tape-aware obstacle rejoin

Log `AAI10-motion-2026-11-01T13-46-23-378Z.txt` shows GP28 black during MOVE_ALONG at nominal along=300 mm, then during FACE_OBJECT. v5.42 ignored these observations because only the dedicated return leg checked for tape. The final halt was NO_ENCODER_PROGRESS_1S during the left turn (L=-894/-994, R=830/994, invalid=0/0), not a three-attempt limit. Wi-Fi timing was healthy (max poll 3959 us, max loop 53674 us). User now confirms the bottle is on a bend; a parallel-return-only assumption is unsuitable.

v5.43 allows either front line sensor to interrupt outward/along movement, inspection turns, scans, settling and the return leg. It first requires both sensors off tape for 100 ms and at least nominal 80 mm outward displacement, to avoid immediately accepting the starting line. A tape candidate stops both motor outputs and recentres the sonar. After 300 ms settling, two consecutive valid forward readings above 30 cm plus retained tape authorize a trial of the existing centre-line follower. Missing echoes do not establish this clearance. A vanished, blocked or unconfirmed (1.5 s) candidate resumes the previous detour phase with the same encoder endpoint, paused timers and zeroed PI output; it must leave tape before another candidate is armed. No barcode input is used in obstacle mode.

Successful rejoin requires at least 1080 forward counts on each wheel (nominal 80 mm) and 300 ms sustained strong centre tape while following. Alignment has a 10-second bound; an obstacle during the trial stops the car. Seeing black alone neither proves the bottle is behind the car nor guarantees a valid route heading. The old fixed axle-advance/original-heading return sequence is removed: return tape also uses this conditional rejoin. When no acceptable tape is encountered, the existing bounded detour and side-clearance checks remain.

Cautious turns only gain a bounded torque assist after 300 ms without encoder progress: up to 30% output, with the existing gradual ramp. It releases on progress; the one-second hard-stall stop remains. Straight detour legs retain their 24% cap and speed control. This addresses the observed low-drive stall possibility without claiming its physical cause is proven.

Validation: sanitized 18-route bypass regression; early rejoin from nine detour phases, right-sensor hint, servo settling, stationary-tape rejection, blocked/lost/no-echo candidate resume with preserved targets, and obstacle after rejoin; simulated turn deadband recovery and hard stall; two straight motor-response models; 44 centred production-main cases; standalone motion regression; wireless core/protocol/concurrent core-1 tests. Strict Pico W build and RP2040 UF2 blocks/version/embedded-page checks pass. Browser interaction was not exercised; page changes are help text, phase labels and version only. Physical test pending. No flash or robot operation performed.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_43_wireless.uf2`, SHA-256 `29669b8ce4e18666447771ae788b8ac7220787dc7f75dfd1046ea004dfc9a5c0`.

## 1 November 2026 — v5.42 independent Wi-Fi servicing and bypass right-turn trim

Physical v5.41 log `AAI10-motion-2026-11-01T13-33-25-021Z.txt` completed the initial right turn at 992/-987 counts and the outward leg at 2698/2701 counts. The user measured the right turn at about 85 degrees. The following left turn was interrupted by MOTOR_SERVICE_TIMEOUT: CYW43 reported STALL/send_ethernet failure, Wi-Fi poll reached 1,099,337 us and the main loop reached 1,115,513 us, exceeding the unchanged 150 ms motor-service deadline. Encoder invalid counts remained 0/0. The left-turn endpoint was not reached; no left target correction is justified from that run.

Moving wireless firmware now initializes and services the polled radio, lwIP, DHCP and HTTP exclusively on core 1, with an aligned 8 KiB stack. Core 0 retains sensor acquisition, motor control, the motor alarm and GP20. Its calibration_wifi_poll only publishes selection availability. Shared log/result/command copies use a mutex with interrupts enabled; no radio, TCP, sleeping or printf operation runs while holding it. Consumed mission configuration is snapshotted so a subsequent request cannot replace an already-consumed command's settings. Wireless diagnostics remain available; a radio stall may delay browser updates or browser Stop, while GP20 and motor control remain local. This is a calibration-firmware concurrency change, not completion of the project RTOS port.

Bypass-only cautious right turns add round(989.5 * 5 / 85) = 58 counts, yielding +1045/-1045 targets. The left target remains -994/+994; standalone calibrated movements retain their original targets/profiles. This empirical trim still needs physical angle confirmation.

Validation: production core-1 transport compiled with actual host threads under address/undefined sanitizers and separately ThreadSanitizer. A simulated 1.1-second radio stall allowed over 240 control updates, concurrent log capture, eventual HTTP Stop delivery and coherent mission settings; a genuine 150 ms control lapse still tripped the guard. HTTP/DHCP protocol suites, 18 bypass simulations (including target check), and 44 production centred main-loop scenarios passed. Strict Pico W build and UF2 structure/family/version checks passed. No firmware flashed or robot operated. Artifact: `build/motion-wireless/robocar_motion_integration_v5_42_wireless.uf2`.

## 1 November 2026 — v5.41 slower, shorter obstacle bypass

Latest physical evidence: `AAI10-motion-2026-11-01T13-17-48-721Z.txt`. v5.40 completed the right turn, the nominal 300 mm outward leg (4092/4053 counts against 4047 targets), left turn, nominal 150 mm along leg and side scan. It did not establish side detection (nearest 89.6 cm, side_seen=0). It stopped with AVOID_ENCODER_INVALID during FACE_ALONG, raw invalid 0/2; a log gap hides the precise second-event timing. The later browser Stop is not the original failure. A wall-triggered end of the outward leg is not supported by the log.

The slow-distance target still used the legacy 25% PWM floor, so overspeed could not reduce drive adequately. Bypass straight legs and bypass-only turns now use a 600 counts/s target (450 close to endpoint), low-speed feedforward/PI, zero minimum, 24% power cap and 4 percentage points per 100 ms upward slew. Turn count targets remain unchanged; reduced coast means their physical angles must be checked. Standalone calibrated motion and course following retain their existing profiles. Encoder A/B reads now use one GPIO register snapshot; this removes the separate-read race, but does not prove the observed invalid transitions were caused by that race. Existing error allowance and stall guards remain.

User reports overall width about 11–12 cm and rear extent about 5 cm behind axle. Initial nominal lateral offset is now 200 mm, extensions and forward checks 100 mm. This is a provisional clearance allowance, not a measured object width. Return requires side detection followed by two consecutive clear spatial checkpoints, with at least two valid far central-ray echoes at each; no-echo alone cannot authorize return. Minimum along distance is 400 mm rather than 600 mm; maximum offset/along remain 600/1200 mm. Bypass deadline is 240 seconds to accommodate slower legs and inspections; individual straight legs remain bounded to 30 seconds. Servo remains within 50–125 degrees, so whole-car side-facing turns are intentional. Return tape must still be observed and confirmed.

Validation: sanitized bypass simulations cover 18 success/failure routes plus two motor-response models for power cap, ramp, overspeed and endpoints; 44 centred and 18 legacy production main-loop cases; standalone calibrated-motion regression; wireless protocol/HTTP/DHCP host tests; strict Pico W build and UF2 block/family/version verification. Browser interaction test could not run in this session because the Node runtime was unavailable; only help text/version changed in the page and its embedded copy was regenerated. No flash or physical run performed. Artifact: `build/motion-wireless/robocar_motion_integration_v5_41_wireless.uf2`. Next physical check: one bottle on a straight floor section, confirm approximately 20 cm outward movement at low speed, side observation, and tape return; report any turn angle error.

## 1 November 2026 — v5.36 Prepare acknowledgement fix

## 1 November 2026 — v5.40 isolated encoder transition during bypass

Physical v5.39 run `AAI10-motion-2026-11-01T13-11-56-117Z.txt` detected the bottle, stopped, and entered TURN_RIGHT_OUT. It aborted on AVOID_ENCODER_INVALID with raw invalid counters 0/1, before any servo scan. User estimated a 45-degree turn. The commanded target was the existing calibrated RIGHT_90; the early exit was an encoder guard, not a completed 45-degree command or sonar stop. The cause of that single invalid quadrature transition is not established.

Scoped to obstacle mode, accept at most two isolated discarded transitions per wheel per whole run, separated by at least 250 ms of controller time. Multi-event jumps, rapid recurrence, exhausted allowance and counter resets still fault. The ISR/raw counts are untouched: no missing counts are invented. After this bounded check, downstream course/motion fault metadata uses the run baseline so the same accepted event is not rejected again. This is a limited tolerance for imperfect observations, not proof that wiring or encoder integrity is repaired. Existing turn targets, stall/direction/time guards, servo sweep, clearance checks and return-to-line logic are unchanged. New ENCODER NOTE, AVOID LEG and retained skip counters expose accepted events and progress toward turn targets. Generic sonar-at-stop diagnostic renamed SENSOR AT STOP to avoid confusing context with the stop cause.

All 44 production centre-main scenarios passed. Sanitized bypass simulations include a complete bypass/return with one right-encoder invalid event during the first turn, burst/rate/total limits, counter baseline and reset handling. Main integration tests prove the single-event case reaches the servo scan, and a burst stops before scanning. Strict Pico W build and UF2 validation passed. Physical bypass remains pending. Artifact: `build/motion-wireless/robocar_motion_integration_v5_40_wireless.uf2`.

## 1 November 2026 — v5.39 distance-based obstacle detection

User screenshots show consistent approximately 30–33 cm and 60–62 cm readings for presented targets. v5.38 logs showed repeated per-measurement 45 ms timeouts, which were incorrectly labelled stuck-high faults and interrupted following. A timeout only establishes that this measurement did not finish within its window; it does not establish permanent sensor failure.

A falling-edge measurement timeout now returns invalid distance with NO_RETURN_WITHIN_WINDOW and no latched error. Existing no-return handling continues line following; it neither reports an obstacle nor supplies a fabricated valid distance. Two consecutive valid distances <=30 cm still stop following and begin the existing bypass. ECHO must be low before a new trigger: HIGH_BEFORE_TRIGGER remains a separately handled stopped retry with a one-second recovery deadline. Short pulses, encoder failures, stale IR and user stop remain unchanged. This policy cannot distinguish all disconnected-sensor cases from open space; no-return is not proof of clearance, and physical obstacle testing remains necessary.

The new continuous 60 ms-pulse regression failed on v5.38 and passes on v5.39. All 42 production centre-main scenarios and sanitized obstacle simulations pass, including no-return continuation, subsequent close-object detection, permanently high input, and unchanged course-follower outputs. Strict Pico W build and UF2 family/block/version checks passed. No physical run or flash performed by the agent. Artifact: `build/motion-wireless/robocar_motion_integration_v5_39_wireless.uf2`.

## 1 November 2026 — v5.38 stopped sonar retry

Three v5.37 runs (`12-40-04-857Z`, `12-41-01-768Z`, `12-42-17-217Z` downloads) all ended with AVOID_SONAR_ECHO_STUCK_HIGH before detecting an obstacle. Runs 1 and 3 entered left line search before stopping; run 2 retained summary shows only 150/143 counts and GP28 still black. New diagnostics show ECHO high at the 45 ms deadline, followed by valid distances again. The actual falling edge beyond that deadline remains unmeasured; the prior 38 ms explanation alone was insufficient for this hardware.

Keep the existing course follower and 45 ms bounded sensor read. A transient stuck-high/high-before-trigger indication now commands zero motor output and waits for two consecutive valid distances, up to one second. Normal no-return readings cannot release this pause. Persistent failure stops with AVOID_SONAR_RECOVERY_FAILED. Controller time freezes during the pause so line search, turn targets and stall budgets are not consumed while stopped; encoder positions and search origins are preserved. Invalid encoders, stale IR, close/bad pulses, user stop and genuinely late control remain faults. Two close readings on recovery trigger avoidance before forward output. Retry/resume counts are retained in the result and transitions logged. This does not make sonar asynchronous or prove physical steering/clearance.

Verification: 41 production centre-main scenarios including transient recovery and persistent faults; sanitized obstacle tests including unchanged course-controller outputs, frozen line search, missing-echo refusal and obstacle-on-resume; wireless tests; strict Pico W build; UF2 structure/family/version validation. Physical test pending; no robot flashed or operated. Artifact: `build/motion-wireless/robocar_motion_integration_v5_38_wireless.uf2`.

## 1 November 2026 — v5.37 sonar no-return timeout correction

Both runs in `AAI10-motion-2026-11-01T12-33-42-847Z.txt` stopped with `AVOID_SONAR_ECHO_STUCK_HIGH` in FOLLOW, before obstacle detection. The centre sensor still saw tape and encoder invalid counters were zero. The old falling-edge deadline was 30 ms; HC-SR04 documentation describes a 38 ms no-return high pulse (https://www.elecrow.com/download/HC_SR04%20Datasheet.pdf). The physical pulse length was not recorded, so this is a reproduced software failure mechanism, not a measured diagnosis of the exact sensor waveform.

The falling-edge deadline is now 45 ms. Finite pulses beyond the supported distance are marked OUT_OF_RANGE; they do not latch a stuck-high fault or become valid clearance measurements. The 30 ms rising-edge timeout, high-before-trigger guard, genuinely stuck-high fault and existing movement guards remain. Stop/live/result logs now include pulse lengths and status.

A production-main simulation of alternating valid and 38 ms no-return pulses failed before the fix and passes afterward; a 60 ms high pulse still stops the car. All 40 centre-main and 18 legacy-main scenarios, obstacle controller tests and wireless transport tests passed. Strict Pico W build and UF2 structure checks passed. Physical retest pending. Artifact: `build/motion-wireless/robocar_motion_integration_v5_37_wireless.uf2`, 945152 bytes; SHA256 `2768ff78ae766ec595b4d190163128d5f31b53db5c454e8915e80b605a23c7a0`.

Reproduced the v5.35 screenshot using the exact acknowledgement extracted from production firmware: successful obstacle selection includes “barcodes disabled”, which the browser's unanchored `disabled` test misclassified as rejection. The firmware had accepted the mode. The page now recognises refusal reasons at the beginning of the acknowledgement and displays the actual reason. Browser regression tests cover the actual success message and real refusal messages; full existing UI scenarios and the strict Pico W build pass. Obstacle movement logic is unchanged. UF2: `build/motion-wireless/robocar_motion_integration_v5_36_wireless.uf2`. No physical robot was operated.

## 1 November 2026 — v5.35 obstacle bypass test

Half-barcode work paused at the user's request. Added a separate right-hand obstacle mode with barcode capture/actions disabled, encoder-controlled turns/straight legs, repeated servo scans within 50–125 degrees, bounded lateral extension and forward traversal, tape-confirmed return, and retained logs. Existing GP28-centred following and GP20/browser Stop remain. Reference comparison, provisional geometry, no-echo limits and first physical test are in [the v5.35 test note](testing/obstacle-v5_35-bypass.md). Software verification does not establish physical obstacle acceptance; no robot was flashed or driven by the agent.

## 30 October 2026 — v5.34 independent optical course capture

Both untouched v5.33 runs accepted no letters; final stops were recovery encoder stall and line not reacquired. v5.34 doubles ADC sampling to 2 kHz, shortens GP26 smoothing from 10 ms to 6 ms, removes centred-course encoder/phase capture resets, and searches successive complete framed-letter candidates. Recovery adds bounded per-wheel effort when encoders show no progress. See [evidence, reference comparison and limitations](testing/barcode-v5_34-independent-optical.md). Sanitized suite,34 centred and18 legacy main scenarios, wireless/actual-lwIP checks, strict Pico W build and UF2 verification pass. Physical validation remains pending; no car operated or flashed. UF2: `build/motion-wireless/robocar_motion_integration_v5_34_wireless.uf2` (924160 bytes). SHA256 `3f12def79c2961d4438417d3e90c1e6a0454f6cfced7d9115104afce3abb3568`. Next: same A then bend/Z course, actions straight, inspect decoded sequence and sampling/recovery results.

## 30 October 2026 — v5.33 wheel-difference steering and recoverable trim timeout

Physical v5.32: two early CENTER_STEERING_NO_RESPONSE stops before any barcode; third run read only A, with25 recoveries and no Z. v5.33 adds bounded20ms differential-travel feedback to the centre drive alongside speed PI, clears obsolete turn demand when the optical request changes, and routes unsuccessful gentle trims into existing bounded recovery rather than immediately ending the run. Actual stall and other guards remain. Barcode decoding/capture unchanged. Diagnostics now expose differential error/duty and trim-response fallbacks.

Sanitized regressions, asymmetric wheel-response simulations,33 centre and18 legacy main scenarios, strict Pico W build and UF2 verification pass. No physical acceptance claimed. See [evidence and test plan](testing/steering-v5_33-wheel-difference.md). UF2: build/motion-wireless/robocar_motion_integration_v5_33_wireless.uf2; SHA256 f017623ae6247ad28ee4b07611ad36e7325835c44034a4d095af41da81af116d. Next: same A then bend/Z route, default straight actions, inspect startup and recovery count as well as accepted sequence.

## 30 October 2026 — v5.32 motion-aware forward barcode capture

Latest challenge log (`docs/testing/logs/20261030-v5_31-bend-A-Z.txt`) decoded neither A nor Z:64 edges,2 phase-discarded partial frames,27 recoveries. v5.32 uses encoder axle-distance widths for centred course scans, preserving frames through forward steering and brief stationary confirmations. Actual >4-count reverse excursions or encoder jumps discard partial scans;2s no-progress and zero-travel-edge guards remain explicit. New logs distinguish these causes. It does NOT reconstruct missing stripes or promise a read while the offset sensor pivots/backtracks across a barcode. See `docs/testing/barcode-v5_32-forward-bend-capture.md`.

Sanitized barcode/motion suite,33 centre-main and18 legacy-main scenarios, mocked wireless core/transport/DHCP checks, strict Pico W build and UF2 verification pass. New spatial Z main-loop case survives zero-power line confirmation. Real curved-track A/Z validation remains pending. No motors operated or firmware flashed. Artifact `build/motion-wireless/robocar_motion_integration_v5_32_wireless.uf2` (919552 bytes), SHA256 7b70ac99305837e079be3d16304d6c0baccb2c206d9e776522d836a423f2b16e.

## 30 October 2026 — v5.31 bounded outer barcode marker tolerance

The latest v5.30 half-A pass captured29 runs with no sampling fault or recovery-discarded fragments, but rejected its end marker: final narrow bar125ms, neighbouring narrow black bars about41–42ms. v5.31 retains strict payload/marker patterns and adds bounded tolerance for ONE expanded physical outer narrow guard bar; it neither guesses A nor repairs missing edges. Bounds: <=3x other corrected narrow median and <=80% of shortest corrected wide run; other elements remain strict. See `docs/testing/barcode-v5_31-half-A-marker.md` for evidence, limitations and tests. Steering and GP26 thresholds remain v5.30 settings.

Exact logged-edge replay now produces A, including through continuous capture. Sanitized barcode/motion regressions, all supported letters/directions and boundary rejection cases pass.32 centre-main and18 legacy-main scenarios and strict Pico W build pass. Physical half-A decoding remains pending. Artifact: `build/motion-wireless/robocar_motion_integration_v5_31_wireless.uf2` (913408 bytes; SHA256 47312c24ae3a2bdbdb6ec6b8ff2093de89d59834a53665f1fcdc7661c5603662). No flashing or car operation performed.

## 30 October 2026 — v5.27 provisional filtered GP26 threshold trial

Six labelled optical captures resolve why a faster poll alone will not help: current251/220 detects mostly wide bars in the full half-A trace; the narrow peaks are near the white threshold. The last capture supports a low-contrast trial, while earlier sweeps have inconsistent contrast and unknown height/angle. See docs/testing/barcode-v5_27-half-A-calibration.md for source windows, deductions, reconstruction limitations and test scope.

Moving GP26 now uses a10-sample mean at1 ms, black entry226 / white release220; legacy stationary sampler defaults stay251/220. Raw optical diagnostics, line-following settings and strict general A–Z decoding remain. Four bounded-window reconstructions recover A with the real filter/sampler/decoder; partial inputs yield no letter. Filtered A–Z synthetic matrix at30/60 ms narrow widths, prior barcode/motion regressions and32 centred main scenarios pass. This is a provisional hardware-calibrated profile, not established moving half-A acceptance or a fix for every capture condition. Next: one moving half-A pass at fixed mounting, then inspect decoded event and retained result.

## 30 October 2026 — v5.30 measured wheel response and balanced restart

v5.29 physical log11-58-31-231Z records10 recoveries,10 trim reversals, zero barcode edges and final BARCODE_ENCODER_STALL on RIGHT (1001 ms without progress, last PWM0.194/0.215). User confirms wheels were free and the car was not handled. The final stall remains a real unresolved physical observation, not assumed to be lifting. During requested left trims the encoders sometimes show greater left than right forward travel, i.e. yaw opposite to the request. After each recovery the left often moves while right barely moves. The450 cps correction trial regressed physically; software tests previously did not model unequal static friction. Stationary black confirmation does not establish vehicle heading.

v5.30 restores600 cps outside-wheel correction target;60–220 cps differential and earlier progress-aware optical trim remain. Centre-profile wheel speed feedback updates at20 ms minimum intervals instead of100 ms (actual loop can be slower). New centre-only start/restart phase verifies at least8 forward counts on each wheel. A wheel ahead by12 counts is unpowered while its partner catches up. Per-wheel effort starts at provisional0.12/0.14 minimums or bounded learned seeds, ramps at0.12 duty/s while that wheel has not reached8 counts, capped0.26. Encoder stall, sampling, timing, travel, button/sonar stop and watchdog protections remain. These seeds are diagnostic tuning, not a newly measured motor model.

After recovery, new_segment preserves bounded learned effort (feedforward plus prior integral, seeds capped0.22) while resetting velocity/count history. It still performs the balanced launch. The PI handoff is primed from startup duty. Optical trim is deferred during this small startup movement. A correction may reverse on worsening/stalled optical progress only after at least20 differential counts in the requested yaw direction. A requested turn that has not physically happened after1 s stops with CENTER_STEERING_NO_RESPONSE; the absolute1.5 s/240-count trim limits remain. This prevents treating failed motor execution as evidence that the optical direction was wrong. No geometric heading estimate or guarantee of post-recovery alignment has been added; physical verification is still required.

Validation: new unequal-friction synthetic wheel-plant tests, opposite-side friction, warm restart, bounded leading-wheel skew, blocked-wheel stops and late-loop checks. Simulated launch peak skew13.2/0.1/27.3 counts and final4 s speeds approximately595/595,599/598,597/587 cps; these are software results only. Recorded opposite-yaw regression confirms no false optical reversal. Sanitized barcode/motion suite,32 centre main and18 legacy main scenarios pass. Strict Pico W build and UF2 structure/family/version checks pass. No car operation performed. Firmware build/motion-wireless/robocar_motion_integration_v5_30_wireless.uf2; SHA256 de95e9323741d3d3b1d257cc911a335dc496f79c022690eec8b14c87513f8d26.

Next physical run should repeat the same straight-edge starting case with fixed sensor placement, default straight course actions. Inspect CENTER_BALANCED_START, actual wheel counts during correction, reacquisition behaviour and final stop. Do not increase PWM or dismiss the encoder/power path if the freely moving right wheel still reports a stall. Barcode acceptance is not the priority of this run.

## 30 October 2026 — v5.29 progress-aware forward steering

Latest runs in the cumulative v5.28 downloads (11-46-02-556Z,11-47-04-155Z,11-48-16-736Z) accepted zero letters and discarded1/1/2 active barcode fragments during recovery. Entire-run recovery counts10/11/15; all final stops CENTER_LINE_NOT_REACQUIRED. All three retained full five-second driving traces with intact timing. Passing the physical barcode is not proof of a complete decoding pass.

Old trim reversed after500 ms or120 differential counts even with improving centre reflectance. Run1 raw GP28 approximately1243→1351→1538 coincides with reversal before loss. v5.29 enters trim at85% of frozen reference, clears at92% (previously80/90). A20-unit normalized improvement renews only a progress timer. Deterioration of more than40 from the peak after80 ms/20 differential counts or500 ms without progress permits one reversal.120 stage counts reverses only a non-improving trial. Absolute1.5 s and240 accumulated differential-count limits span both trial directions and cannot be renewed by progress. Exhaustion enters the existing bounded search.

During trim the outside target drops from600 to450 cps; the tapered difference is60–220 cps, retaining positive inside targets and wheel-speed feedback. Straight travel remains600 cps. Corrected straight-speed statistics: the legacy1080 cps cutoff wrongly classified all600 cps cruising as steering; centre mode now uses540. Motor PI gains, manoeuvres, barcode thresholds/decoder, search protections and automatic driving traces remain unchanged. A centre-only reading still lacks a left/right sign; GP27 or last successful direction supplies the initial trial.

Validation: sanitized barcode/motion regressions; recorded-value steering regression; improving/stalled/deteriorating trials; absolute budgets despite progress; correction slowdown/return and straight speed statistics.32 centred and18 legacy production-main scenarios pass. Strict Pico W build and UF2 checks pass. No physical test performed. Firmware: build/motion-wireless/robocar_motion_integration_v5_29_wireless.uf2, SHA256 b8323e0093fc761b1a69f2069b4e076c84beb3bf3df1330aafbf549b728e9733.

Next: one same-track half-A driving pass with fixed mounting, default straight actions; stop before table edge, download promptly. Compare actual interruptions, reversals, wheel-speed summaries and decoded sequence. Physical steering improvement and half-A acceptance remain unverified.

## 30 October 2026 — v5.28 automatic driving optical diagnostic

Physical v5.27 evidence: `AAI10-motion-2026-10-30T11-35-45-086Z.txt` contains one course run, zero accepted barcodes, 24 edges and 23 completed runs (29 required for a framed single character), one incomplete-frame timeout, no acquisition integrity fault, max sampling gap1033 us and no fragments discarded for recovery. One early black interval lasts572 ms; this suggests missed/merged transitions but the sparse live readings cannot establish which threshold or optical effect caused them. The final stop is `CENTER_LINE_NOT_REACQUIRED`, after eight successful reacquisitions; it is not an obstacle stop. Whether the final failure occurred at the end of the physical tape is not known.

v5.28 is a diagnostic build, not a verified half-A decoding fix. Reuses the existing2 KiB raw min/max buffer to capture five seconds from the first detected barcode edge in each driving scan. Prepared in foreground with IRQ exclusion; constant-time trigger and bounded extrema collection in the existing ADC IRQ. Only one capture per run; an early spurious edge can trigger it, so use one isolated barcode. Records min/max over10 ms windows, not every raw sample or exact within-window ordering. No pretrigger history is retained. Capture freezes when motion ends; early stops are marked aborted/incomplete. Data is emitted after stopped settling, with `source=DRIVE` and trigger offset relative to run start, into the same Download log. No extra buttons or hand swipe. Download promptly, with the page open: full trace retention still depends on the rolling log/browser buffer.

Threshold226/220, ten-sample filter, decoder checks, motion tuning and all safety limits remain as v5.27. Stationary capture remains available. A new motion run clears the old retained optical summary. Browser recognises a saved driving trace without telling the user to slide paper.

Validation: sanitized acquisition/decoder/motion and optical arm/trigger/freeze tests pass;32 centre production-main scenarios pass, including full/early-ended driving captures, stationary cancellation and assertions that optical logging occurs outside IRQ with zero motor PWM. Wireless transport/retention and mocked browser tests pass, including driving trace in the common download. Strict Pico W build and UF2 block/family checks pass. No hardware flash or physical test performed. Artifact: `build/motion-wireless/robocar_motion_integration_v5_28_wireless.uf2`, SHA256 `7a605d8c76a4f9c0d966da3ac65db569f7c81530e98c3c4128fcdef97c6deb1e`.

## 30 October 2026 — v5.26 simplified calibration UI

The user's v5.25 download AAI10-motion-2026-10-30T10-58-58-268Z.txt contains the full optical capture:5000 samples, raw172..2782, maximum gap1017 us, no empty10 ms windows, no abort. All500 extrema windows are present. The first roughly2.4 s stay near250..278, followed by two prominent high peaks, then mostly low readings. Capture integrity is confirmed, but this unlabeled sweep does not establish narrow-bar separation or a complete decoded barcode. No threshold or driving change is justified from this trace alone.

v5.26 replaces the long instruction page with one test dropdown and three visible buttons: mode-specific Record sensor / Prepare run, Stop, Download log. Defaults to motors-off barcode sensor check. Motion options, full Y/Z actions and junction advance remain accessible; Y/Z settings are collapsed and only relevant during track mode. One shared download includes optical and motion records; redundant recovery button removed, retained results load on reconnect/test completion and at download. Live technical log is collapsed, sensor summary is readable text and the graph scales to phone width. Physical GP20 still starts driving; stationary optical capture starts from its explicit Record button, never starts motors, and GP20 cancels it. Motor, line-following, decoder and sensor thresholds unchanged from v5.25.

Validation: browser mocks exercise all six test choices, all retained action dropdown options, numeric validation, default straight mission, optical trace fragments, one combined download, automatic result retrieval, busy/offline lockout, Stop, reset clearing, responsive graph and three-button layout. Wireless transport/retention tests and strict Pico W build passed. No physical flash or motor run performed. Artifact: build/motion-wireless/robocar_motion_integration_v5_26_wireless.uf2.

## 30 October 2026 — v5.25 stationary optical trace; half-barcode contrast unresolved

User reports v5.24 completed the home route. Latest stationary screenshots show GP26 wide white185–190, narrow white200–218, wide black1067–1081, narrow black214–219. Narrow classes overlap. No defensible separating threshold follows from these samples; black entry251 and white release220 remain unchanged. GP28 following, frozen reference, search bounds, speed and mission behaviour are preserved. Do not label this as a half-barcode decoding fix.

Added web button “Inspect barcode signal · 5 seconds” (serial v): stationary capture only, no GP20 start required. Existing1 ms ADC IRQ records min/max in500 successive10 ms windows; brief peaks are preserved while exact within-window pulse timing is not. Fixed trace buffer is approximately2 KiB. No heap allocation, alternate ADC owner or ISR logging. Foreground prints bounded numeric trace lines and summary after capture, showing sample count, missing windows, maximum sample gap and abort flag. Sampling_ok means uninterrupted capture, not readable barcodes. Browser graphs the extrema and includes them in Download log; latest optical summary is separately retained through browser disconnect, but the full trace relies on the rolling log/browser download. Download immediately before another capture or power reset.

Captures are rejected during movement, countdown or settling. Physical GP20 and browser stop cancel a capture without starting a move. Selection and terrain calibration are blocked while capturing. Capture ends after5 s even if the ADC timer stops. Inspect the half-size strip in a slow stationary swipe to check for peaks missed by the250 ms screen or imperfect alignment, without guessing missing stripes. No automatic threshold adjustment or relaxed decoding introduced.

Validation: sanitized optical envelope tests for brief peaks, missing windows, clock wrap and cancellation; barcode/motion regression suite;32 centre production-main scenarios including optical timed completion and GP20/browser cancellation with zero motor PWM; wireless command/retention/HTTP tests; mocked browser trace parsing across fragmented log responses, graph, download, busy/disconnect/reset handling and mobile width. Strict Pico W build and UF2 structure/version checks passed. Physical optical-sweep and half-size decode validation remain pending. Artifact: build/motion-wireless/robocar_motion_integration_v5_25_wireless.uf2.

## 30 October 2026 — v5.24 stable tape reference and stopped confirmation

Latest v5.23 evidence (`AAI10-motion-2026-10-29T14-31-32-896Z.txt`) shows three `CENTER_LOCAL_RECOVERY_EXHAUSTED` stops. In the first, the running maximum reference rose to797; normal-looking GP28 raw1919 remained below its approximately2026 reacquire threshold. Another run entered confirmation at raw2002 then exhausted recovery; soft-boundary/coast interaction is a supported code failure mode, not proven from the sparse live samples alone. Barcode results A / empty / empty, without rejection, timeout or integrity faults.

v5.24 removes running-maximum adaptation. During the stopped countdown, collect the last31 normalised GP28 observations at least10 ms apart. Median becomes the fixed per-run reference; require all31, last sample no older than100 ms, median>=400 and interquartile range<=20 percent of median. Weak/unstable placement rejects start as `CENTER_BASELINE_NOT_READY` and prints median/spread/count. Reference is set once at mission start, with no running optical sample allowed to raise it. Retained course result includes fixed reference.

A strong candidate must first appear inside the existing sweep travel budget. Motor command then stays zero throughout confirmation. Permit up to80 extra counts of coast per wheel from candidate detection, require50 ms settled (movement beyond2 counts resets this timer) and30 ms continuous black, with300 ms confirmation timeout and existing3 s total search limit. During this stage use70 percent reference as black hold threshold instead of85 percent entry; both have normalised300 floor. Excess coast, lost/unsettled confirmation or late control stop with distinct reasons. No powered extension of the sweep and no success on black first seen beyond the sweep cap. Retained result reports per-wheel confirmation coast. This is a bounded software allowance, not a measured physical stopping-distance guarantee.

Validation: new sanitized regressions for median/outliers, stale/weak/unstable baseline, baseline replacement, frozen reference under log-like797/637 readings, successful zero-power confirmation after crossing the soft60-count boundary, lost black, excess coast and late control.29 centred production-main scenarios, including startup spike rejection by median, frozen reference during higher running readings and unstable-start rejection;18 legacy scenarios; existing barcode/motion suites, wireless retention/transport, mocked browser page/download and strict Pico W build/UF2 checks passed. No physical acceptance yet. Barcode capture/thresholds and mission action semantics unchanged. Next physical check is the same full A then half A route, Y/Z straight, GP28 centred and stationary for countdown; download before restarting and stop before lifting. Confirm stable tracking first, and require actual `sequence=AA` before claiming both symbols were read.

## 29 October 2026 — v5.23 early optical steering and local recovery

Supersedes v5.22's immediate barcode-area stop. Latest log `AAI10-motion-2026-10-29T14-13-45-254Z.txt` has three preserved stops with `CENTER_LOST_NEAR_BARCODE`; first retained run read one A, later stops had no complete symbol. Latest stopped after18 edges, with zero rejection/timeout/integrity fault. The issue was controller policy interrupting a partial scan after GP28 loss, not a decoder-requested stop. v5.22 only maintained straight encoder balance while GP28 saw black; it lacked early optical steering.

v5.23 keeps GP28 centre / GP27 right / GP26 barcode mounting. It learns a strong GP28 reference (normalised500 minimum,900 maximum; update during following with GP27 white). Forward correction starts below80 percent, clears above90 percent. It slows the inside wheel by100–300 counts/s while the outside target stays600, keeping both targets positive. GP27 black is a rightward hint; otherwise use the last successful direction or probe left initially. A worsening reading after80 ms and20 differential counts reverses the trial. Each leg is limited by120 differential counts or500 ms, with one reversal; exhaustion enters bounded recovery. Centre weak plus right black no longer counts as a crossing; strong centre plus right black remains a provisional crossing candidate. Barcode capture remains active during forward trim.

Full line loss still brakes. Automatic searches normally allow120 wheel counts first sweep /240 after reversal. Recent GP26 activity now tightens that to60/120 instead of immediately stopping; further activity never resets the search origin/budget. Failure reports `CENTER_LOCAL_RECOVERY_EXHAUSTED`. Strong confirmation requires85 percent of the reference (normalised400 minimum) for30 ms. Encoder travel/direction limits are checked before accepting black. Commanded-turn reacquisition remains400/800. Pivot/search fragments remain discarded; a narrow recovery can therefore save tracking without preserving an interrupted barcode. This does not identify ink semantically or guarantee clearance on a track of unknown spacing.

Diagnostics add `CENTER_TRIM_LEFT/RIGHT`, reference, requested steering difference, trim attempts/reversals in live and retained results; line-presence logging uses the same hysteresis state as the controller. Dedicated barcode thresholds, calibrated fixed-distance/angle routines, GP20/web stop, obstacle and motor/sampling fault protections retained. No specific home-track corner positions or expected A letters are encoded.

Validation: sanitized controller/barcode suites;27 centred production-main scenarios including left/right trim during a complete barcode, bounded failed local recovery, successful short recovery followed by a subsequent valid barcode;18 legacy main scenarios; wireless retention/transport and mocked browser checks; strict Pico W build/UF2 validation. Physical outcome pending. Flash v5.23 and run the same full-A then half-A pass with Y/Z set straight. Confirm smoother tracking and examine sequence, trim/recovery counts and stop reason; do not assume half-A success without `sequence=AA`.

## 29 October 2026 — v5.22 centre-edge recovery and barcode-zone guard

Evidence: `AAI10-motion-2026-10-29T13-47-18-338Z.txt` contains three preserved GO sections plus log gaps, so the four user-reported tests cannot all be mapped confidently. First complete and latest retained results each say `reads=1 sequence=A timeouts=1 edges=42`, with maximum sample gaps 1029/1026 us and no integrity fault. First A at 10745 ms was followed by only 12 further edges and a timeout: no demonstrated full-A/half-A pair. Another preserved run has one A, 22 recoveries and an obstacle stop. User observed edge stutter and a recovery onto barcode ink. Home tape-to-barcode gap approximately 25 mm; school gap unknown.

Changes: GP28 follow-presence hysteresis uses normalised 250 enter / 200 leave (approximately raw861/724 with existing calibration). Search confirmation requires normalised400 (raw1270) continuously for30 ms, instead of accepting the weak edge as found. This moves the acceptance point further into black; it does not geometrically measure the tape midpoint. Automatic loss sweeps reduced from400/800 to120/240 counts, still bounded by timing, wrong-direction and per-wheel progress checks. GP26 black or confirmed transitions timestamp recent barcode activity; automatic line loss within2 s stops as `CENTER_LOST_NEAR_BARCODE`, including activity detected during a search. This prevents that automatic search, not all possible barcode/tape confusion. Conservative guard may stop at a genuine bend beside a barcode, and a barcode not seen by GP26 cannot activate it. No assumed school spacing or course-specific turn position. Explicit commanded-turn reacquisition retains400/800 bounds and is not covered by the automatic-loss guard.

Barcode thresholds and1 ms sampling unchanged; incomplete frames are never guessed. Added failure widths, and retained final-result fields for last ended capture widths, active run count and discarded fragments during braking/search. These remain available when live log history is lost. `last_ended_*` refers to the last completed/timed-out/rejected capture, not necessarily a particular physical sheet.

Validation: sanitized controller/barcode suites including threshold hysteresis, strong/flickering reacquisition, shorter search bounds, guard before/during recovery, expiry, and synthetic full A followed by half A;24 centred and18 legacy production-main scenarios, wireless retained-result/transport checks, mocked browser downloads/lockout/mobile layout; strict Pico W build and UF2 structure/version validation. No new hardware pass yet. Full-size A physically demonstrated; half-size A remains unresolved. Next: flash v5.22, same mounting; one full-A then half-A pass with straight exit, both Y/Z actions straight. Stop with GP20 before lifting; download before another run. Observe whether edge chatter improves or the explicit guard stops the run. Success requires `sequence=AA`, not merely visible transitions.

## 29 October 2026 — v5.21 GP28 centre-line mounting trial

User explicitly chose GP28 directly over tape, GP27 to its right, GP26 dedicated to barcodes. Build enables ROBOCAR_CENTER_LINE. Legacy straddling code remains available only in a separate build; its moving b/c/t/n diagnostics are rejected in this build and removed from the page. Fixed-distance/angle diagnostics remain. Startup mission requires fresh GP28 black. UI and log roles identify centre rather than line-left.

Centred tracking uses encoder-controlled 600 counts/s. Centre loss brakes immediately and pauses barcode interpretation. GP27 black selects right-first search; otherwise it probes left, then reverses. Provisional search bounds: 400 counts first sweep, 800 on return, 1.2 s per sweep, 3 s total, 50 ms braking before each direction, 18 percent opposed PWM, per-wheel 1 s progress check and wrong-direction checks. Resume requires GP28 black for 30 ms, with reset odometry segment; a timeout never resumes forward driving. Search also replaces post-manoeuvre reacquisition in the centre build. Latest deferred instruction activates on recovery; STOP overrides. Recovery count/layout are retained in the downloaded result.

Both GP28/GP27 black still denotes a crossing candidate, not proof: wide bends and lateral spacing can cause false positives. No claim of physical junction-count acceptance. Sensor thresholds/height, search angle/power, speed and barcode optical contrast remain physical trial items. After relocation keep original forward lead/height; test one bend with a straight exit before barcode runs. No circuit-specific turn positions or barcode meanings added. No code copied from the external reference project.

Validation: sanitized controller and recorded barcode suites; 21 centre-layout production-main scenarios (including successful/failed recovery and start rejection), 18 legacy production-main scenarios; wireless retained-result/transport tests; mocked browser selection, downloads, lockout and mobile layout. Strict Pico W build and UF2 checks. Physical centre-layout validation pending.

## 29 October 2026 — v5.20 steering power retention and stall evidence

User reports a tight left correction followed by a stop. Fixed repeated clearing of positive speed-loop integral during nonzero inside-wheel commands; this could prevent recovery power from accumulating during gentle corrections. Intentional zero-target pivots still clear power and integral. No unmeasured minimum-duty floor introduced. Speed targets, barcode decoding, crossing policy and one-second encoder-stall guard retained.

Added wheel-specific stall mask, no-progress ages, last commanded targets/PWM and last measured speeds to the retained MOTION STOP line, so the download retains evidence even with live Wi-Fi log gaps. This identifies the monitored wheel; it does not by itself distinguish a stuck wheel from a missing encoder signal. Diagnostics use the controller segment time, including after manoeuvres.

A bilateral repeated-edge regression fails on v5.19 and passes with the fix; checks also verify the one-second stall still fires, an intentionally stopped inside wheel is exempt, and the driven outside wheel remains monitored. Sanitized controller/barcode suites, 18 production mission-loop scenarios and wireless result/transport tests passed. Strict build and RP2040 UF2 structure/version checks passed. Physical benefit remains pending; exact cause of the previous stop is not proven. Flash v5.20, select Y/Z mission with both actions straight, run through the same bend/barcode, stop with GP20 before lifting and download the log.

## 29 October 2026 — v5.19 no overall course timer

User requested removal of the course timer. Removed only the overall course wall-clock cutoff; retained 160000-count travel bound, GP20/web stop, obstacle and all motor/sampling/manoeuvre fault checks. Latest school log `AAI10-motion-2026-10-29T07-07-35-729Z.txt` actually stopped on OBSTACLE_STOP at 29.9 cm, not time. Five crossing candidates, zero commanded turns, zero valid barcode reads, one rejected capture, nine timeouts, max sample gap 1051 us with no integrity fault. Raw transitions do not establish complete barcode capture; live-log gaps prevent full reconstruction. Speed/thresholds unchanged from v5.18. Sanitized unit suites and all 18 production main scenarios pass; added past-five-minute controller check while retaining travel/stall tests. Strict build and RP2040 UF2 validated; hardware retry pending.

## 29 October 2026 — v5.18 straight-speed trial

School log `AAI10-motion-2026-10-29T06-56-44-021Z.txt` ended with COURSE_TIME_LIMIT after 120 seconds, four crossing candidates, zero commanded turns and zero invalid encoder transitions. User reports successful following, wrong direction for barcode placement. Increased continuous/course straight target to 1200 counts/s only after 250 ms without an edge; bends and course crossing/centering remain at 900 counts/s, with prior tight-corner inside-wheel cutoff. Course test cap is now 300 seconds, retaining 160000 counts and existing stop protections. Unit/decoder/guard/motion suites and 18 production main-loop scenarios pass; new tests verify delayed cruise and immediate edge slowdown. Strict build and RP2040 UF2 validated. Straight target +33 percent is not a measured lap-time gain; no guarantee of corner or barcode performance at this speed. Flash/retest pending.

## 29 October 2026 — v5.17 removes barcode-white start gate from line-following runs

School log `AAI10-motion-2026-10-29T06-35-24-740Z.txt` showed successful Y/Z selection and three GP20 countdowns rejected by the barcode-white gate (later background readings around 300–360 versus the 220 start threshold). Y/Z mission and Continuous scan now start with fresh IR sampling regardless of barcode colour. Decoder waits independently for 250 ms white, avoiding a partial starting symbol; single-pass barcode diagnostic retains its white-start requirement. Start rejection now sets an explicit stopped reason instead of leaving MOTION_COUNTDOWN. v5.16 steering retained. Strict build and all 18 sanitized production main-loop scenarios passed, including permanent black, observed-like 330 background, and black-start followed by white and a valid Y. UF2 validated; hardware retest pending.

## 29 October 2026 — v5.16 physical corner tuning candidate

Two v5.15 Continuous scan runs understeered on the real track. Built v5.16 with a shared 900 counts/s continuous/course profile, prompt inside-wheel slowdown, an 80 ms sustained-edge drive cutoff and a bounded 100 ms correction hold. Crossings clear correction memory; calibrated manoeuvre targets and Y/Z route semantics remain unchanged. Software checks passed; actual corner improvement remains unverified until the next run. See [corner evidence and retest](testing/course-v5_16-corner-steering.md).

# Development and test status

**28 October: v5.15 Y/Z-triggered mission built; physical validation pending.**
Replaces the run-relative selector with independent Y and Z dropdowns: immediate
turns, future-junction turns, skip-then-turn, straight/cancel and stop. New reads
replace waiting instructions; committed manoeuvres defer the newest valid read;
Stop overrides. Barcode capture pauses during pivot/braking/search. User-defined
test meanings only. [Policy, controls and tests](testing/course-v5_15-barcode-actions.md).

**28 October: v5.14 school-course test built; physical validation pending.**
Separate mode adds stronger curve steering, debounced crossing encounters and an
optional one-shot left/right turn at encounter 1–9. User measured line sensors
120 mm ahead of axle; nominal centering remains adjustable and unvalidated.
Default straight-through mode records A–Z; no invented barcode mission meanings.
Controller/main-loop simulations, regressions, wireless/UI tests and strict build
checked. [Scope and school test order](testing/course-v5_14-school-test.md).

**28 October: v5.13 continuous scan built; physical validation pending.**
New separate mode records successive letters and continues after reads/rejections,
rearming after a 250 ms white gap. Brief both-black readings use bounded slow
straight bridging; persistent ambiguity and existing fault/obstacle guards stop.
20-second/24,000-count test bounds remain. Software/build/mock UI checks passed.
No mission turns, bypass or cliff detection. [Procedure and scope](testing/barcode-v5_13-continuous.md).

**28 October: v5.12 physical A and Z decode; repeated central-line stops.**
A1 and Z2 decoded. The Z export contains five attempts; Z1/Z3/Z4/Z5 stop on
BOTH_LINE_BLACK before a complete frame, with no decoder/capture fault. User
emphasizes continuous assessment operation; the current single-frame diagnostic
is not mission firmware. [Evidence and scope](testing/barcode-v5_12-physical-line-stops.md).

**28 October: v5.12 decoder build ready; physical check pending.** Per-symbol
black/white timing-bias correction reads nine recorded complete A/Z frames,
including the v5.11 Z1/A2 rejections. Strict separation, coherence, exact guards
and failed-scan stops remain. Steering/thresholds unchanged; negative-pattern,
packed-storage, capture/motion/wireless tests and build pass.
[Method and checks](testing/barcode-v5_12-width-bias.md).

**28 October: v5.11 failure stops physically verified; decoding still mixed.**
Z outcomes reject/read/read; A outcomes read/reject/timeout/read. User identifies
A3 as deliberate misalignment. Full-frame Z1/A2 failures reproduce the decoder
width-ratio guard; all sampling/encoder fault counts are zero.
[Evidence and next investigation](testing/barcode-v5_11-Z3-A4-result.md).

**28 October: v5.11 built, physical validation pending.** Moving barcode tests
stop on the first rejected frame/element timeout; steering uses ramped900/1200cps
targets with continuous feedback instead of55% outer-wheel PWM. Raw-gap counters
and target logs added. Recorded-run replay, host regressions, strict build and
mocked UI checks pass. [Details](testing/barcode-v5_11-failure-stop-steering.md).

**28 October: v5.10 barcode repeatability remains open (3/5).** A1/Z2/Z3 decoded;
A2 captured a complete frame but failed a width-ratio check; Z1 missed transitions
and timed out. Both failures continued until BOTH_LINE_BLACK, not fall detection.
Sampling/encoder faults were zero. [Analysis](testing/barcode-v5_10-A2-Z3.md).

**28 October: v5.10 physical speed/obstacle check completed.** Mean clear-interval
wheel rates1172/1155cps versus1200; obstacle stop at sonar27.2cm; user reports33cm
travel. No encoder/capture faults. Sonar long-range outliers remain unresolved.
Next: A/Z decoding at the revised speed using the same firmware.
[Evidence](testing/barcode-v5_10-speed-obstacle-result.md).

**28 October: v5.10 speed-feedback build ready; physical test pending.**
Barcode-pass starting power and speed feedback revised using v5.9 logs; retained
line corrections, barcode thresholds and calibrated manoeuvres. Build, host
regressions and mocked browser tests pass. Next: combined line/speed/box run.
[Changes and procedure](testing/barcode-v5_10-speed-feedback.md).

**28 October: v5.9 obstacle stop physically demonstrated.** User confirms line
following and approximate20–25cm obstacle clearance. Log stops on OBSTACLE_STOP
at sonar26.7cm, settles near25–26cm with zero encoder/capture faults.30cm is a
two-reading trigger, not a guaranteed final clearance. Speed tracking remains
open; sonar outliers and full bypass are not validated by this run.
[Evidence](testing/barcode-v5_9-obstacle-stop.md).

**28 October: v5.9 physically decoded A and Z successfully.** One user-labelled
pass each,30 transitions,1 accepted frame,0 rejected frames and no encoder or
capture faults; automatic stop on BARCODE_READ. A log confirms restored0.25/0.55
left correction, but visual tracking/right correction still need confirmation.
Current-version repeatability tally A1/3,Z1/3. Speed target tracking remains open.
[Evidence](testing/barcode-v5_9-A-Z-success.md).

**28 October: A barcode decoded successfully in two v5.8 driving runs.** Both
captured30 edges with no sampling/encoder faults and stopped on BARCODE_READ.
User reports weak line following; logs show detected left edges with too-small
steering differential. v5.9 restores immediate0.25/0.55 line-edge response,
preserving barcode thresholds and calibrated manoeuvres. Software/build checks
pass; physical correction and Z decoding pending. Measured wheel speeds exceed
the nominal1200cps target, so speed regulation is not yet validated.
[Evidence and change](testing/barcode-v5_9-line-priority.md).

**28 October: v5.8 controlled-drive barcode firmware built.** Barcode pass now
regulates encoder speed at a nominal1200cps with251/220 thresholds and retained
edge traces. Original motion calibration and bounded stops preserved. Build,
controller/acquisition/regression/transport and mocked-browser tests pass.
Physical movingA test pending. [Build and run procedure](testing/barcode-v5_8-controlled-drive.md).

**28 October: v5.7 now captures complete barcode patterns physically.** Two
hand swipes each produced30 edges with no data faults. Both rank as reversed*A*,
but both fail the current width-separation check; no letter accepted yet.
Controlled-motion validation is next, rather than more identical hand swipes.
[Capture and decoder replay](testing/barcode-v5_7-two-complete-frames.md).

**28 October: v5.7 fixes a reproduced stationary timing-check race.** v5.6
physical capture aborted at6.492s before the user's barcode sweeps; this is not
a threshold-validation result. New build takes a coherent timestamp snapshot
and logs fault details, preserving251/220 thresholds. Eighteen host scenarios
and build/transport checks pass; physical test pending.
[Evidence and fix](testing/barcode-v5_7-freshness-fix.md).

**28 October: v5.6 barcode threshold built, physical test pending.** User-labelled
readings show narrow black254–260 versus white182–202. Changed GP26 barcode
black-on to251 (>250), white-release to220, retaining two-sample confirmation.
Sixteen simulated stationary scenarios and regression/build checks pass. v5.5
physical scans captured six black regions per slide and no complete barcode.
[Changes, evidence and next run](testing/barcode-v5_6-threshold.md).

**28 October: v5.5 stationary barcode diagnostic prepared.** Dedicated firmware
holds both motors off, permits slow hand scans, and retains up to128 transitions
across decoder timeouts. Physical validation pending. Previous v5.4 hand slides
while idle did not decode; latest recovered result was stale. Moving capture and
recurring encoder errors remain unresolved. [Procedure and tests](testing/barcode-v5_5-stationary-scan.md).

**28 October: v5.4 barcode diagnostic built; physical test pending.** Added 1 ms
alarm-owned ADC acquisition, a bounded timestamped edge queue, and a separate
Barcode pass (read / 8 s / 8000-count stop). New summaries expose sampling gaps,
queue overflow and captured widths. All four motion targets retained. First run:
large A, then large Z and half-sized samples. [Procedure and evidence boundaries](testing/barcode-v5_4-moving-pass.md).

**v5.3 physical line check:** seven logged runs, six travel-limit stops and one
intentional both-black stop, all with zero invalid encoder transitions. User confirms
correction toward the line from angled starts; smoothness remains open. Apparent
pitch spans -16.5 to +9.3 degrees, with no completed new humps; flat-surface/handling
confirmation pending. No tuning changed. [Evidence](testing/line-v5_3-physical-results.md).

**v5.3 role mapping completed:** user confirms GP26 offset barcode, GP28 line-left,
GP27 line-right. Firmware isolates these roles and adds a GP20-started, two-second
line check with a wheel-count cap and stop conditions. Barcode detections are record-only;
all four motor targets are unchanged. Software checks passed; physical line steering
is pending. [Details and next run](testing/ir-v5_3-role-mapping.md).

**Latest stationary IR check:** all three electrical channels show clear light/dark
contrast in the unordered sweep. Existing thresholds separate the observed low/high
clusters. Physical role mapping still needs two identified placements (offset barcode,
central-left); do not infer roles from sweep order. No firmware change required for
that check. See [contrast evidence](testing/ir-v5_2-unordered-sweep.md).

**Latest v5.2 physical result (26 October):** user confirms right90 corrected;
completed right90 and straight runs show zero invalid encoder transitions. Keep
motion targets unchanged. Straight pass shows strong contrast on IR_C only. User
clarifies two central line sensors plus one offset barcode sensor; current firmware
still uses a three-channel barcode vote, so channel-to-role identification and
logic adaptation are required before line-following integration. See
[run evidence and next check](testing/motion-v5_2-right90-ir-pass.md).

Last updated: 26 October 2026 (historical component results retained below).

**v5.2 update:** new physical v5.1 log contains completed L90, two R90 and two R180
runs, all with zero invalid encoder transitions. User reports straight30 and L90/R180
good, R90 about 85 degrees. v5.2 adds a provisional +58-count R90-only trim, combined
IR/sonar/pitch/run timing summaries, removes UI feedback fields and adds an independent
motor service alarm after an observed idle radio stall. Other targets unchanged.
New physical verification pending. See [v5.2 evidence](testing/motion-wireless-v5_2-combined-check.md).


**26 October physical v5.1 update:** wireless page selection and log export now
worked on the car. The supplied session contains three movements aborted by sonar
OBSTACLE stops (18.3, 25.8, 25.0 cm) and two right-180 starts blocked immediately
(17.7/18.1 cm). None completed its target. User estimates of 15 cm/45 degree travel
are observations of aborted moves, not evidence for changing distance or turn
calibration. Investigate sonar detection/mounting and clear-space operation first.
See [the preserved log analysis](testing/motion-wireless-v5_1-obstacle-stops.md).


**26 October follow-up: v5.1 DHCP correction built.** v5 associated with the
Mac but failed address assignment (self-assigned 169.254 address). An actual-lwIP
packet regression reproduced the initial 0.0.0.0 source being dropped before
DHCP. The corrected server-port accept configuration passes the complete DHCP
exchange in that test. On-car v5.1 verification remains pending; motor tuning
is unchanged. See [wireless calibration](wireless-calibration.md).

**26 October: v5 wireless calibration built**, continuing the v4 30 cm/end-pivot
check with unchanged motion tuning. The car hosts its own protected Wi-Fi network
and browser page; USB/VS Code output remains available. Host logic/transport tests,
browser tests with a simulated car, and firmware compilation passed. Physical
Wi-Fi/battery behaviour and the v4 balancing result are still pending. See
[wireless calibration](wireless-calibration.md). This diagnostic build is not the
final MQTT/RTOS integration or the complete minimum-bit implementation.

Implementation update, 21 October 2026: the existing Batch 5 controller has now
been ported into the main firmware, with USB diagnostic selection and packed
barcode capture. Host checks and the Pico W build passed; broader physical
integration testing remains pending. The verified results below remain the historical
hardware baseline. A subsequent first physical straight run completed with
settled counts 4272/4144 and zero invalid transitions. The operator estimated
about 35 cm; no measured distance was provided, so distance accuracy remains
open. See [run evidence](testing/motion-integration-2026-10-21.md) and
[motion integration](motion-integration.md). Do not
restart completed component calibration solely because this build is new.

A subsequent ruler-measured run travelled 340 mm for the 300 mm command,
with settled counts 4289/4276 and zero invalid transitions. v2 applies a
provisional straight-only ground scale (12.595588 counts/mm; target 3779)
and checks endpoints each loop. The subsequent supplied log contains two
completed runs, with means 3871.5/3869.5 counts and zero invalid transitions;
the operator reported 320 mm travel. v3 now uses a provisional 3537-count
30 cm endpoint, with physical verification pending. See
[measured run and correction](testing/motion-integration-distance-v2.md).
See also [v2 results and v3 endpoint](testing/motion-integration-distance-v3.md).

The operator then reported 30 cm with minimal error on v3, with settled counts
3631/3627 and zero invalid transitions. A small end-left pivot was observed;
the left wheel had led by up to 94 counts before the final catch-up. v4 keeps
the 3537-count endpoint and adds bounded straight-wheel balancing and cutoff
timestamps. Its physical result remains pending. See
[v3 observation and v4 change](testing/motion-integration-stop-yaw-v4.md).

## Verified on the assembled robot

### Motor bring-up

- Both motors have been driven in the expected forward direction.
- The right motor polarity was corrected and reverified during the raised-wheel
  encoder motor test.
- GP20 start/stop behaviour was demonstrated in the integration prototype.
- Both encoder A/B channels have been hand-rotation tested with no invalid
  transitions. Forward count signs are left negative and right positive.
- A repeated ten-revolution hand test measured approximately 2539 counts per
  revolution on the left and 2547 on the right; 2543 is the initial shared
  calibration value.
- At equal 35% PWM with both wheels raised, the left wheel measured about 3229
  counts/s and the right about 3393 counts/s. The right wheel was 4.9% faster;
  both directions were correct and both invalid-transition counts remained zero.
- The standalone PI controller targeted 3000 counts/s and measured 3000 counts/s
  left and 3017 counts/s right over eight seconds. It reduced the mismatch to
  0.5% using approximately 32.5% left and 30.8% right PWM, with zero invalid
  transitions. Integration into the main motion module remains pending.
- The Batch 5 floor test completed 30 cm straight, right 90 degrees, left 90
  degrees, and right 180 degrees in one run. The operator reported that the
  motions looked good. Final straight counts were 4255 left and 4217 right;
  peak wheel-count skew during travel was 56 counts. All four stages reported
  zero invalid encoder transitions. This is a visual floor check, not a
  measured-angle or full-course acceptance test.

### Ultrasonic sensor and servo

- HC-SR04+ distance readings respond to suitable front-facing targets.
- Coarse and fine servo scans complete successfully.
- The scan range is restricted to 50-125 degrees to respect the mechanical installation.
- Small or edge-on objects may reflect sound away from the receiver and return `no echo`.
- The calculated scan footprint is not yet a calibrated physical object width.

### GY-511 accelerometer

- I2C communication and live acceleration readings are working.
- Automatic two-second level calibration is working.
- Calibration is rejected when the car moves excessively during sampling.
- Climbing, descending and level orientations are distinguishable.
- A state machine detects normal and back-to-back hump sequences.
- Incomplete or timed-out sequences are cancelled.
- Hump count and highest climb angle are reported.

Bench demonstration result:

| Hump | Peak climb | Peak descent | Duration |
| --- | ---: | ---: | ---: |
| 1 | +24.3 degrees | -5.2 degrees | 3110 ms |
| 2 | +36.6 degrees | -6.4 degrees | 8292 ms |

The second hump was correctly reported as the highest climb angle. Actual hump-height estimation and real-track tuning remain pending.

### IR sensors and line following

- All three analogue IR sensors have been measured on the light styrofoam and dark line.
- Physical left/centre/right order is Grove 5/7/6, using GP26/28/27 respectively.
- Three-sensor following, GP20 start/stop, short-gap crossing, and bounded lost-line recovery are implemented.
- Serial diagnostics report the run state, line state, sensor readings, drive commands, range, and terrain status.
- Straight-line bench testing has begun; full-course line and junction performance is not yet verified.
- A motor-disabled line/junction diagnostic now records stable three-sensor
  patterns and flags broad `111` regions as junction candidates for physical
  course testing.

### Barcode and navigation commands

- A reusable command module maps the brief's example symbols A-D to left,
  right, straight and U-turn requests.
- A compact Code 39 decoder recognises one A-Z letter between `*` start/stop
  markers from 29 alternating bar/space widths. It accepts either travel
  direction and uses relative widths rather than a fixed speed or scale.
- A fixed-size reader captures full-width black/white transitions using a
  two-of-three sensor majority and suppresses repeated commands for three
  seconds. Sustained junction-like readings time out without producing a
  barcode command.
- Host tests pass for A-D navigation commands, the supplied A and Z samples,
  live capture, scaled and reversed input, duplicate suppression,
  invalid-pattern rejection and junction timeout.
- Left, right and U-turn commands carry requested angles for the future motion
  subsystem. Until that subsystem consumes them, the integrated firmware
  stops safely and reports the requested turn; straight commands continue.
- Sensor thresholds, the 750 ms edge timeout and the 3 s duplicate window
  still require physical validation on the printed course.
- Accurate command execution remains dependent on integrating the calibrated
  encoder-based distance and turn control into the main firmware.

### Integrated safety behaviour

- The robot starts with both motors stopped and waits for GP20.
- Two consecutive valid ultrasonic readings below 30 cm stop and disarm the motors.
- A missing ultrasonic echo is treated as unknown, not as a confirmed clear path.
- The sonar stop is not an obstacle-bypass implementation.

## Standalone firmware tests

| Program | Purpose |
| --- | --- |
| `robot_bringup` | Combined hardware connectivity and manual actuator checks |
| `servo_range_test` | Establish safe servo travel |
| `ultrasonic_scan_test` | Coarse/fine obstacle scans and profile diagnostics |
| `imu_hump_test` | Automatic level calibration and multi-hump detection |
| `ir_calibration_test` | Capture light/dark values for three IR sensors |
| `encoder_wiring_test` | Motors held off; report A/B edges and quadrature counts while each raised wheel is turned by hand. Hardware verified; no invalid transitions observed. |
| `encoder_motor_test` | With wheels raised, run left, right, and both motors at 35%, followed by a 3000 counts/s PI-controlled stage. All stages hardware verified. |
| `motion_control_test` | Defaults to one run of straight 30 cm, right 90 degrees, left 90 degrees, and right 180 degrees; individual motions remain selectable with GP21. Each completed stage is saved to flash and the full report is replayed after reconnection. Batch 5 completed all four stages; the operator reported good straight and turn behaviour, and every stage had zero invalid encoder transitions. This standalone calibration is ready for main-firmware integration. |
| `line_junction_test` | Motors held off; record stable three-sensor patterns and junction candidates while the car is moved manually over the course. |

## Next development priorities

1. Check the newly ported speed and turn control in the main robot firmware
   against the existing Batch 5 baseline; repeat calibration only if a specific
   fault or changed hardware provides a reason.
2. Test the three-sensor line follower and recovery on the actual track; tune speed and thresholds.
3. Validate barcode navigation on the physical course; implement obstacle bypass and Wi-Fi/MQTT telemetry.
