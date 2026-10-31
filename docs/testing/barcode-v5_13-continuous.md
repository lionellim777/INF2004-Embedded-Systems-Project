# v5.13 continuous scan integration test

Built 28 October 2026. Physical validation pending; no flashing or robot operation performed.

## Behaviour

The new **Continuous scan** selection (`c`) follows the line and records successive
supported single-letter Code 39 A–Z barcodes. A successful read, rejected frame or
element timeout does not stop motion. The capture waits for 250 ms of continuous
white before rearming; a distinct repeat of the same letter can be recorded.
The bounded event list retains up to 16 letters, and overflow stops the run.

Brief simultaneous black readings from both central line sensors invoke a straight
900 counts/s bridge. This is bounded by 300 ms or 360 counts per wheel. Recovery
requires 50 ms without both sensors black. Persistent ambiguity stops the run.
This is experimental straight-track bridging, not junction navigation.

The run ends on GP20/browser abort, existing obstacle/fault guards, 20 seconds or
24,000 wheel counts. It does not stop merely because A or Z was read. The original
single-scan Barcode pass remains separate. Decoder, reader, encoder, line-check
and motor-guard sources match v5.12; calibrated manoeuvre targets are retained.

The first 128 raw transitions are retained for diagnostics. Truncating this trace
is nonfatal in continuous mode; sampler overflow, sample-gap and event overflow
remain faults. The final `[BARCODE RESULT]` retains the complete bounded sequence.

## Next physical check

1. Flash `build/motion-wireless/robocar_motion_integration_v5_13_wireless.uf2`.
2. Reconnect to AAI10-Calibrate, open http://192.168.4.1/ and confirm v5.13.
3. Put A then Z beside a straight line, at least 5 cm of plain white between
   barcode edges. Keep the entire route supported and clear of platform edges.
4. Start the barcode sensor on white before A. Select Continuous scan, wait for
   SELECT, then press GP20. Expect A and Z events while motion continues.
5. Press GP20 after Z, wait for the settled result, then download the log.
   Expected sequence for a successful pass: `reads=2 sequence=AZ`.

Neither the line sensors nor sonar provide reliable cliff detection. This build
does not implement mission completion, barcode-directed turns or obstacle bypass.
The time/count caps are test bounds, not protection against reaching a platform edge.

## Software verification

- Strict Pico W firmware build passed.
- Barcode suite passed, including continuous A/Z/repeated-letter capture, quiet
  gaps, rejection/timeout recovery, clock wrap, event overflow, line ambiguity,
  count/time bounds and existing recorded-frame/motion/guard regressions.
- Wireless host suite and all 18 stationary actual-main scenarios passed.
- Mock browser checks passed for the continuous command, retained sequence,
  download, running lockout, reconnection and mobile layout; rendered UI inspected.
- UF2 block structure, RP2040 family and version banner verified; previous v5.12
  UF2 preserved. Build hashes are in `build/motion-wireless/build-report-v5_13.json`.

These are software checks, not physical acceptance evidence.
