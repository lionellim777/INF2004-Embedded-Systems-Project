# v5.4 moving barcode pass — 28 October 2026

Status: **built and host-tested; physical validation pending**. This is a diagnostic
firmware, not the final RTOS or full-course navigation. No robot was flashed or
started by the agent. Existing uncommitted work and versioned UF2s were preserved.

## Why this change

The user has printed large and half-sized A/Z. The old v5.3 line check has a
2-second / 3537-count cap. Its foreground barcode reader can miss transitions
while sonar blocks: prior physical logs measured a 38.476 ms maximum loop gap.
Increasing travel alone would not fix that acquisition gap.

## Test setup and workflow

1. Flash `build/motion-wireless/robocar_motion_integration_v5_4_wireless.uf2` with
   motor power off, using BOOTSEL and RPI-RP2. Existing v5.3 remains a rollback.
2. Power from the secured bank. Keep still and level at startup. Join
   AAI10-Calibrate / AAI10-drive, open http://192.168.4.1/ and refresh to **V5.4**.
3. Use a continuous level surface with **at least 1 m clear ahead**, away from
   foam edges or steps. Sonar detects frontal obstacles, not a drop-off.
4. Put **large A** beside the straight route, so GP26 (leftmost offset sensor)
   crosses each bar and space. Bars run across the travel direction. GP28/GP27
   remain either side of the route line. Tape flat outside all sensor paths;
   avoid labels, glare and wrinkles. Begin with GP26 on white ~5 cm before the
   first bar. The two line sensors must not run over the barcode itself.
5. Select **Barcode pass · A / Z**, wait for `[SELECT] BARCODE_PASS`, then GP20.
   After the three-second countdown it moves. GP20 also aborts locally.
6. Expected: `[MOTION STOP] ... result=BARCODE_READ`, followed by
   `[BARCODE RESULT] decoded=A valid=1`, after settling for 500 ms. It records
   the letter only, without turning. Z should similarly say `decoded=Z valid=1`;
   it is a supported barcode character, but has no current navigation action.
7. Download each run before the next, and report which sheet was used. Once the
   large A works, repeat separately for large Z, small A, small Z. If it stops
   before the barcode ends, misses the bars, oscillates badly or reports a wrong
   letter, send that run rather than repeating the same failure across all four.
   Do not lift/reposition until the settled result is printed.

The prepared four-page A4 PDF has 235 / 117.5 mm scanning spans and 5 / 2.5 mm
narrow elements when printed at actual size. Its 100 mm ruler checks scaling.
These are home calibration sizes, not confirmed course dimensions.

## Implementation and limits

- A 1 ms repeating alarm owns all three ADC channels (eight reads averaged per
  channel, 5 us mux settling). Foreground reads a coherent snapshot. No other
  ADC owner may be added without changing this ownership scheme.
- GP26 barcode edge hysteresis: black ON >=935 raw; white OFF <=800 raw.
  Two successive samples confirm a transition; retain the first crossing time.
  This is a provisional filter using the existing measured contrast, not a
  newly measured calibration of paper under current lighting.
- IRQ writes a bounded 64-edge buffer. Foreground takes a batch with IRQs briefly
  masked, then decodes outside the interrupt. No printing, decoding, network
  operations or mutexes occur in the ADC callback. The existing packed 20-bit
  run-width reader remains 88 bytes; the diagnostic queue is additional RAM.
- Timestamped edges preserve timing during main-loop sonar/Wi-Fi delays. Samples
  are scheduled at 1 kHz, not guaranteed at precisely 1 kHz: max sample gap,
  max ADC callback duration and queue peak are logged. A >3 ms sample gap,
  stale sampling or buffer overflow stops the barcode pass and invalidates the
  capture. Actual alarm/encoder contention still needs on-car measurement.
- `b` selects BARCODE_PASS (browser or USB). GP20 is the only start. Requires a
  fresh white GP26 reading at countdown completion. A supported decoded character
  stops this test; no turn or mission command executes.
- Drive: nominal 0.40/0.40 PWM; left-line correction 0.35/0.45, right-line
  correction 0.45/0.35. No speed regulation is claimed. Both central sensors on
  black stops. Both white remains ambiguous (line centred OR absent).
- Bounds: 8 s or 8000 forward counts on either wheel; stop on invalid encoder
  transition, wrong direction below -64 counts, or either wheel making no count
  progress for 1 s. Counts are a limit, not a promised travel distance. Existing
  sonar <30 cm twice, GP20/browser abort and 150 ms motor-service guard remain.
- All four calibrated motion targets, gains and PI are retained: straight
  3537/3537, left90 -994/+994, right90 +987/-987, right180 +2087/-2087. The old
  2-second line-check remains separately selectable.
- Every pass also records encoders, both line sensors, sonar, pitch and loop/Wi-Fi
  timing. Foreground `[CHECK]` IR edge totals can undersample bars; use the
  dedicated `[BARCODE RESULT]` edge count and recorded widths for barcode work.
- Retained result adds the barcode summary to stop/settled/check/timing. It
  survives page reconnect, not power loss or a subsequent run. Five maximum
  lines fit the 4096-byte HTTP body; host transport test checks this boundary.
- Partial captures report complete widths accumulated so far; successful/rejected
  frames report 29 widths. Multiple attempts retain the latest completed/active
  frame, not a history of every frame. `frames`, `rejected`, `timeouts` are counters.
- Encoder callback latency, decoding across true track speed/lighting, false
  positives from unrelated markings, stopping/coasting distance and final line
  recovery/junction handling remain unvalidated. This does not implement RTOS.

## Verification

- `python3 tools/test_barcode_calibration.py`: actual sampler and decoder under
  ASan/UBSan with A/Z, both temporal scales/directions, 40 ms or whole-frame
  foreground delays, jitter, timestamp wrap, bounce, queue overflow, missed
  sample fault, timeout and drive bounds. Existing line-check, motor guard,
  diagnostics and motion tests also pass.
- Barcode storage regression: 156 valid barcodes, 5000 reference comparisons,
  10000 neighbour checks, timeout/wrap/duplicate and Cortex-M0+ layout checks.
- Real wireless handlers with mocked network: new `b` selection, retained barcode
  line, worst-case five-line result, command restrictions, partial TCP, DHCP and
  malformed-packet regressions.
- Mock browser: new button, download including barcode result/version, running
  lockout, reconnection/power-reset labels, desktop/mobile layout, no JS errors.
- Full Pico W wireless and USB builds with strict compiler warnings. UF2 format,
  RP2040 family, flash addresses and v5.4 banner checked separately.

No above test substitutes for the four printed-sheet runs. Source hashes, UF2
hash/size and ELF sections are in `build/motion-wireless/build-report-v5_4.json`.
