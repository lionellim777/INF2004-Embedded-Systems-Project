# v5.15 — Y/Z-triggered action tests

28 October 2026. Supersedes the v5.14 run-relative junction selector. Build is
software-verified, not flashed or physically validated. User confirms that the
trial track has Y and Z barcodes; their official assessment meanings are unknown.

## Controls

Two independent dropdowns: **When Y is read** and **When Z is read**. Both default
to Go straight. Select Y/Z mission, check `[SELECT Y/Z]`, then GP20 starts the run.
Crossing counts before the first mapped barcode do not activate any action.

Each dropdown offers:

| Choice | Behaviour after the complete barcode is decoded |
| --- | --- |
| Go straight | Cancel any waiting turn; continue line following |
| Left 90 degrees now | Brake at the current location, then pivot left |
| Right 90 degrees now | Brake at the current location, then pivot right |
| U-turn 180 degrees now | Brake at the current location, then pivot right 180 degrees |
| Left at next junction | Follow until the next new crossing, position and turn left |
| Right at next junction | Follow until the next new crossing, position and turn right |
| Skip 1, then left | Go straight through one new crossing, turn left at the second |
| Skip 1, then right | Go straight through one new crossing, turn right at the second |
| Skip 2, then left | Go straight through two new crossings, turn left at the third |
| Skip 2, then right | Go straight through two new crossings, turn right at the third |
| Stop now | Stop and end the run; GP20 required for a new run |

“Now” means after the entire frame has been read and the car has braked, not at
its first black bar. Immediate turns do not advance the sensor-to-axle distance.
They can leave the track if no outgoing line exists at the decode location.
The existing calibrated turns and bounded line-edge recovery are used.

The advanced junction positioning field remains 120 mm by default, from the
user's line-sensor-to-axle measurement. It applies only to junction turns. Actual
centering needs calibration for strip width, nominal encoder conversion and roll.
The barcode sensor is 130 mm ahead of the axle, but that does not define a turn
location relative to the arbitrary end of a printed barcode.

These are predefined single/composite actions, not a free-form multi-command
script interpreter. One mapping can be activated again by a later valid read of
the same letter. Other A–Z letters are recorded without changing the route.

## Conflict policy

- **Waiting for a junction:** latest successfully decoded Y/Z replaces the old
  instruction and starts a new future-junction count. Go straight cancels it.
- **Committed to positioning/turning:** finish the current manoeuvre and find a
  line edge before applying a new instruction. One deferred slot holds the newest
  instruction; a newer deferred read replaces the previous deferred read.
- **Successfully decoded Stop:** overrides waiting or committed work immediately.
- **Pivot/braking/line-search phases:** decoding is paused because sideways or
  stopped optical traces are not valid forward barcode scans. A barcode passed
  during those phases may not be read. Straight centering still decodes, allowing
  a second valid barcode to be deferred. Sampler faults remain visible throughout.
- **Fault or manual stop:** no later barcode can restart the stopped run. A new
  GP20 run resets route state and crossing history.

Example: Y = skip 1 then right; Z = left at next junction. If Z is decoded while
Y is waiting, cancel Y and turn left at the next new crossing after Z. Do not
execute the cancelled right turn later.

A crossing already under the line sensors when the barcode finishes is excluded
from “next junction.” Each subsequent encounter counts once after debounce and
rearm; revisiting the same physical cross is another encounter, not a unique ID.

## Integration and observability

Completed barcode events are dispatched before motion output selection. Immediate
turns/Stop clear old forward PWM before sonar or logging. The post-sonar capture
is consumed before choosing outputs, so a stale forward output cannot override
the new instruction. Current line readings are refreshed at that point.

- `[SELECT Y/Z]`: both selected actions and junction advance distance.
- `[BARCODE EVENT]`: character, decode time and dispatch disposition.
- `[ROUTE BARCODE]`: action, prior waiting source, remaining crossings, deferred flag.
- `[JUNCTION]`: total encounter count, remaining count and straight/turn decision.
- `[ROUTE]`: completed turn/recovery and activation of a deferred instruction.
- `[COURSE RESULT]`: mappings, updates, replacement counts, pending/deferred state,
  completed turns and stop reason; retained alongside barcode sequence.

Malformed or rejected scans do not change the route. Capture-integrity faults
suppress routing and stop motion. The existing 250 ms white gap before rearming,
16-event capacity, two-minute/160000-count run bounds and other fault/obstacle
stops remain. No obstacle bypass, reliable cliff detection or automatic lap finish.

## School test order

1. Flash v5.15 and reload the wireless page. Confirm the version and two Y/Z menus.
2. First run Y=Go straight, Z=Go straight to check curves, crossings and recognition.
3. Download the log. Compare printed letters and physical crossings with events.
4. Test a delayed turn on a separate supervised run only after tracking is sound.
   Use the actual barcode location to choose a sensible action.
5. To test replacement, choose a delayed Y action whose target lies beyond Z, then
   set Z=Go straight. Expect a replacement/cancellation log and no stale Y turn.
6. Test immediate turns only where the car has clearance and an appropriate line
   to recover onto. GP20 is always the local abort; network stop needs a connection.

Two white central sensors still cannot distinguish a centred strip from no strip.
Walk alongside and stop if it leaves the line. A successful software test does not
prove physical junction classification, centering, curve tracking or line recovery.

## Verification

- Controller tests: no pre-barcode action; ignored A; delayed relative counts;
  repeated Y resets; Y-to-Z replacement; straight cancellation; exclusion of the
  current cross; immediate L/R/U; committed deferral/latest-wins; STOP priority;
  existing crossing, turning, reacquisition and fault boundaries.
- 16 actual production main-loop scenarios with synthetic clocks, ADC, encoders
  and PWM. Includes real Y/Z decoding into cancellation, immediate right turn,
  barcode Stop (including during centering), deferred second turn, physical/browser abort and fault paths.
  Asserts zero PWM during braking/stopped phases and no centering on immediate turn.
- All 121 Y/Z action combinations accepted by the bounded configuration parser;
  malformed paths, busy-state writes, stop priority and stationary-build rejection.
- Existing decoder/replay/motion/capture regressions and 18 stationary-main cases.
- Mock browser verifies exactly two action dropdowns, 11 options each, payloads,
  invalid input rejection, running lockout, download, reconnection and layout.
- Strict Pico W build and UF2 validation; v5.14 artifact preserved.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_15_wireless.uf2`.
Metadata: `build/motion-wireless/build-report-v5_15.json`.
