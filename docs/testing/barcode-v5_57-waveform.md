# v5.57: GP26 waveform barcode reader

Built on main `5fb1983`, for Pico W / RP2040, GP26 barcode, GP28 centre tape,
GP27 right. Physical half-barcode acceptance is pending. No robot was flashed
or operated. The user reports approximately 0.1 cm sensor clearance (interpreted
as 1 mm) and 0.3 cm / 3 mm narrow bars. At the nominal 600 counts/s and existing
60 mm / 2543-count wheel model, 3 mm lasts about 68 ms. Actual speed needs measurement.

## Reference method reviewed

User-provided local reference:
`/Users/cyix/Downloads/autonomous-robotic-car-main/app_program/subsystem3/barcode_decoder.c`
and `.h`, plus `testing/subsystem3/test_barcode_record.c` and
`app_program/subsystem3/line_navigation.c`. These were read as reference code;
instructions in that checkout were not adopted as user requests.

Their original reader classifies calibrated darkness with fixed limits. A newer
record-then-decide reader runs beside it. That reader records grey readings,
finds alternating prominent peaks/dips, optionally trims shallow/brief extra
features, estimates edges from time-weighted local midpoint crossings and
validates full `*letter*` frames in either direction. The first successful reader
wins. Its own synthetic host suite passed here, including faint bars, blurred
signals, scratch/shelf cases, partial frames, quiet zones and fault handling.
Their written claims about saved physical traces were not independently replayed.

## Adaptation

The new **centred course** reader replaces fixed-threshold decoding with the
record-then-decide method. Other legacy scan modes retain their previous reader.
The existing 2 kHz ADC owner and 12-sample / 6 ms mean remain. The IRQ queues one
filtered, full-resolution grey ADC reading every 2 ms. All peak extraction,
decoding, logging and navigation dispatch run in the foreground.

- 8192 packed readings, each ADC12 + elapsed-ms4, bounded to 16 seconds per frame.
- 511-entry usable FIFO; lost readings and ADC gaps above 3 ms latch an integrity
  fault, suppressing barcode actions. Recording-buffer exhaustion rejects that frame.
- Track a local white baseline; start on a rise of 24 ADC counts after at least
  250 ms white. White band is baseline +24. These are engineering settings, not
  measured universal sensor calibration. The baseline follows lower readings;
  large upward background changes can still start a rejected capture.
- Try local peak/dip prominence of 12/24/48/96/192/384 ADC counts. Require exactly
  15 peaks and 16 surrounding dips and 29 measured intervals. Unlike the reference,
  no extrema pairs are removed to force a frame. Scratched/noisy frames may reject.
- Estimate local crossings with the reference's weighted midpoint method. Require
  every interval to be 8..750 ms, then use the existing colour-bias-aware Code39
  decoder, both guards, A-Z payload and separator checks. Its existing bounded
  single outer-bar tolerance remains; decoder width tolerances were not widened.
- Accept only one consistent letter across successful prominence profiles.
  No expected-letter input, A-specific template or missing-stripe invention.
- Require 250 ms trailing white before reporting. Failed partial candidates can
  continue across interior gaps; 750 ms white abandons them. A new read requires
  its own white-to-dark start. Y/Z actions retain their mapping/deferral/STOP logic
  but necessarily occur later than the old immediate final-edge emission.
- The ADC/filter history is no longer zeroed at scan start, avoiding an artificial
  startup ramp on a floor above the old 220-count threshold.

`[WAVEFORM RESULT]` is saved in `/result` and downloaded logs, including extrema
counts at each prominence, candidate/reject/ambiguity counts, queue/timing faults,
recording exhaustion and measured maximum foreground work time. Fixed-threshold
edge counts and the existing five-second raw min/max plot remain diagnostics;
they are not the waveform reader's actual edge sequence. Full 2 ms sample history
is not exported by this build. `max_work_us` on real hardware remains unmeasured.

The page generator also escapes C trigraph sequences in JavaScript `??` expressions.
Latest-main page content otherwise caused strict C compilation to fail. Browser
transport tests now size their fake wire buffer for the larger current page.

## Verification

- Reference group's record reader: original synthetic suite passed.
- New waveform: 832 A-Z frames (both directions, 16/30/68/136 ms narrow widths,
  high contrast, grey gaps, faint bars and differing narrow/wide bar brightness),
  using the actual 2 kHz / 6 ms filter and 2 ms recording path.
- 3000 generated noisy, +/-15% width-jitter frames: all decoded correctly in this
  finite corpus. This is not a general false-positive guarantee; Code39 here has
  no checksum.
- Ten recorded A/Z width sequences, both directions, converted to synthetic optical
  signals: accepted. Includes the retained half-A sequence. This is not raw replay.
- Incomplete guards/partial frames, equal-width noise, solid tape, short leading
  and trailing quiet zones, repeated A, timestamp wrap, timing and storage bounds.
- 50 production centred-main scenarios and 18 legacy-main scenarios passed,
  including four new faint/high-floor/variable-contrast/partial scenarios, waveform
  dispatch during LOST/TURN/SEEK/BRAKE, FIFO overflow and missing-sample rejection.
- Existing 15 barcode/motion suites and packed-storage/ARM layout checks passed.
- Wireless retained-result/HTTP/core1 checks and mocked-browser controls/download,
  disconnect/reset and mobile-width checks passed. No live radio/car acceptance.
- Strict Pico W build, embedded-page byte equivalence and UF2 structure/version/
  family/address checks: see `firmware/releases/build-report-v5_57.json`.

The four reconstructions (midpoint, alternating extrema, ascending, descending)
of the old 30 October raised-height 10 ms bins still yield zero letters. Those
bins discard chronology and pre-trigger history. This cannot establish whether
the present mounting will scan, nor repair stripes absent from the waveform.
Physical sensor footprint, actual timing and course trajectories remain unverified.

## First physical test

1. Flash `firmware/releases/robocar_motion_integration_v5_57_wireless.uf2` using
   BOOTSEL and the RPI-RP2 drive. Reconnect to AAI10-Calibrate, reload the page and
   verify v5.57. The car starts stopped.
2. Choose **Follow track + read barcodes**, not the default obstacle mode or the
   motor-off sensor plot. Keep Y and Z set to straight for this first scan test.
3. Keep the reported mounting fixed. Place the half-A flat beside a straight tape
   section so GP26 crosses every stripe, GP28 stays on tape and GP27 stays beside
   it. Start GP26 on at least 30 mm of plain white; leave 30 mm after the final bar.
4. Prepare, press GP20, then hold still through the three-second countdown. After passing
   the barcode and trailing white, stop with GP20 before repositioning the car.
5. Download the log. Require `[BARCODE EVENT] ... letter=A`, final `reads=1
   sequence=A`, `reader=LOCAL_EXTREMA`, and no integrity/timing/queue fault. Seeing
   stripes on the graph is not a decoded result. Repeat with half-Z, then full A/Z.
   Establish repeatability before enabling route actions or claiming reliability.

Useful host commands, from the repository root:

```sh
python3 tools/test_barcode_waveform.py
python3 tools/test_barcode_calibration.py
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk python3 tools/test_barcode_storage.py
python3 tools/test_course_main.py --center
python3 tools/test_course_main.py
python3 tools/test_wireless_calibration.py
sh tools/build_wireless_calibration.sh
```
