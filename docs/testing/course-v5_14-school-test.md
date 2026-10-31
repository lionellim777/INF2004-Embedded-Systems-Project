# v5.14 school-track preparation — 28 October 2026

Physical status: **not flashed or tested on the robot**. This is an experimental
course-test build, not a claim of assessment readiness or completed RTOS integration.

## Confirmed user inputs

- Tomorrow's available track is a flat looping line with two physical crossings
  and two side barcodes (photograph supplied by the user).
- The two line sensors are approximately **120 mm ahead of the wheel axle**.
- The offset barcode sensor is approximately **130 mm ahead of the axle**.
- Official barcode meanings and mission instructions are not yet available.

## Capabilities and boundaries

The decoder remains the physically exercised single-letter Code 39 A–Z decoder,
with the same optical thresholds, width-bias compensation and strict guards.
It is not a universal scanner: numbers, punctuation, multi-character payloads,
other 1-D symbologies, QR codes and extended ASCII are not supported by this build.
Basic Code 39 itself has a broader alphabet; do not confuse the symbology's
capabilities with this firmware's subset. Reference implementation alphabet:
https://github.com/zxing/zxing/blob/master/core/src/main/java/com/google/zxing/oned/Code39Reader.java

Course mode records letters and does not give them invented navigation meanings.
Legacy navigation mappings A/B/C/D = left/right/straight/U-turn remain separate.
The course configuration is one optional left/right turn at encounter 1–9,
counted from GP20 start. It executes once; later crossings are straight-through.
This implements the scheduling needed to skip crossing 1 and turn at 2 or 3.
It does not implement arbitrary instruction strings or sequences of route steps.
A later mission layer can translate official barcode meanings into route requests.

A crossing is a *candidate*: both line sensors black for at least 20 ms. It counts
once, then rearms only after both-black has cleared for 100 ms and both wheels
have advanced 600 counts beyond the clear point. Returning to the same physical
cross counts again. The robot does not identify/map physical junctions or laps.
Both-black passage is straight; more than 1.2 seconds or 1200 counts stops it.
Broad markings, approach angle and curve geometry can cause missed/false candidates.

Course steering uses 1000 counts/s outside and 350 counts/s inside a curve, with
2500 counts/s/s target slew and the existing wheel-speed feedback. This stronger
curve correction is provisional, not physically tuned on the school bends.
Two white line sensors still cannot distinguish a centred strip from absent line.
The operator must supervise; there is no reliable cliff or universal line-loss detector.

## Optional turn sequence

1. Skip earlier candidates and count the chosen encounter.
2. Advance approximately the configured distance from the first both-black sample.
   Default 120 mm uses the measured sensor lead. Conversion uses nominal 2543
   counts/revolution and a 60 mm wheel, approximately 1618 counts for 120 mm.
   Crossing width, slip and residual roll affect actual centering; adjust on track.
3. Stop for 250 ms, then use the existing calibrated left/right 90-degree controller.
4. Stop for 250 ms after the turn, then look for a single-sensor black edge.
   If both sensors are white, a low-duty pivot searches left (up to 220 counts),
   then right (up to 440 counts), at most 2 seconds total. Both black is ambiguous
   and stops; no edge found also stops. The search itself needs physical validation.
5. When an edge is found, reset the straight/curve velocity segment and resume.

Barcode fragments are discarded during centering, turning and line seeking.
Capture resumes after a fresh 250 ms white gap. Barcode decoding continues while
following and going straight across crossings. Sampler failures remain fatal.

## Wireless controls and limits

Select **Course action**, **Turn at crossing encounter**, and **Advance before
turn (mm)**, then **Select course test**. No selection starts motion. Wait for
`[SELECT COURSE]`, then press GP20. Settings are rejected while running/settling;
stop takes priority. Power reset clears the selection. Do not start on a cross.

Limits: 120 seconds or 160000 net wheel counts per wheel from run start (not a
path-length measurement during pivots). Existing obstacle, motor-service, encoder,
stall, sampling and browser/GP20 stops remain. No obstacle bypass or automatic
mission/lap completion. At most 16 barcode events are retained; overflow stops.
The original short diagnostic modes and their limits remain available.

## Tomorrow's order

1. Flash `build/motion-wireless/robocar_motion_integration_v5_14_wireless.uf2`.
   Reconnect to AAI10-Calibrate and reload http://192.168.4.1/. Confirm v5.14.
2. First use **Straight through every crossing**. Start on a straight section,
   line between the central pair, barcode sensor over white. Choose a travel
   direction that puts the side barcodes under the offset sensor. Keep the page
   open for logs. Walk alongside, ready to press GP20.
3. Observe the first curve and first crossing. Stop immediately if tracking is
   wrong. Verify one `[JUNCTION]` per crossing encounter, and decoded letters
   against the printed samples. Download after the settled result. No need to
   run until the time cap or complete a full lap to get useful evidence.
4. Only after straight-through tracking and counting agree with observations,
   try a separate experimental turn run. Prefer encounter 1 first to check axle
   centering/turn/reacquisition, then 2 or 3 to check skipping. Use the same log.
   An unsuccessful turn is calibration evidence, not a reason to invent acceptance.

Result includes `[COURSE RESULT] encounters=... turns=... plan=... at=...
advance_mm=... outcome=...`, alongside the retained barcode sequence and timing.
Tell us whether it tracked curves, counted each crossing once, read the printed
letters, and where/why it stopped. No UI feedback form is required.

## Software verification

- Course controller tests: glitches, crossing latch/rearm, repeated encounters,
  nth left/right action, centering, actual calibrated controller targets, bounded
  line search, one-shot consumption, time/count/control/encoder/stall faults.
- Actual production main loop with synthetic hardware: continuous read across
  crossings, browser/GP20 stops, obstacle, sampling, watchdog, invalid encoder,
  stalled wheel, selected turn/resume, abort during turn, failed line search and
  start-on-black rejection. Simulated hardware is not physical acceptance.
- Existing decoder/recorded captures, acquisition, motion, guard and stationary
  main regressions; wireless API and retained maximum-size result tests.
- Browser course settings, invalid inputs, download, disconnect and running
  lockout tested with mock responses; rendered UI checked.
- Strict Pico W build and UF2 validation, with v5.13 preserved.

Build metadata and hashes: `build/motion-wireless/build-report-v5_14.json`.
