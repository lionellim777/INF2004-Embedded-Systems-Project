# v5.5 dedicated stationary barcode diagnostic — 28 October 2026

Status: built and tested on host; physical scan pending. No flash, serial monitor
or robot movement performed by the agent. Previous v5.4 UF2 retained unchanged.

## Why a separate firmware

Latest supplied `AAI10-motion-2026-10-28T05-16-33-105Z.txt` is an idle hand sweep:
400 live rows, all Run=0, GP26 range178–2632. Its recovered result is the previous
moving run (17edges, no decode), not this sweep. v5.4 only decodes while a moving
barcode/line-check mode is active. A repeated failure to explain and implement
that distinction led to unnecessary user testing. This diagnostic makes manual
scanning explicit and prevents motor motion entirely.

Prior stationary observations support black/white contrast, but moving runs
still miss transitions. Right encoder invalid transitions have recurred and the
short platform has caused a roll-off. Neither issue is resolved by this build;
they are excluded from this optical diagnostic and remain pending.

## Flash and use

1. BOOTSEL-flash `build/barcode-stationary/robocar_barcode_stationary_v5_5.uf2`.
2. Join AAI10-Calibrate (AAI10-drive), open http://192.168.4.1/, refresh to V5.5.
3. Keep car stationary and paper flat at normal sensor height. Put the offset
   GP26 sensing tip over plain white before the first bar. Select Stationary scan
   and press GP20. Wait3s for `[SCAN READY]`.
4. Slide large A smoothly once, in one direction, crossing every bar/space in
   about5–15s. Do not pause at each bar, cross the printed letter, sweep across
   bar ends or lift the sheet toward the sensor while scanning.
5. `decoded=A valid=1` means the full pattern decoded. On success the scan ends
   automatically. Otherwise finish the sweep on white and press GP20 again or
   click Finish scan. Download the log even if no letter was produced.
6. Download before a new scan or power-off. A later scan clears the old result.
   Try Z/half-sized sheets only after reviewing the first complete diagnostic.

## Implementation boundaries

- Dedicated `stationary_barcode.c`, selected by ROBOCAR_STATIONARY_BARCODE.
  It does not link motion_control, encoders, terrain or navigation sources.
- Motor GPIO8/9/10/11 are initialized LOW as digital outputs and repeatedly held
  LOW. No PWM configured or nonzero motor write exists. Servo signalGP12 also
  stays LOW. Browser/USB driving commands are ignored, even from a cached page.
- Only GP26 ADC is sampled: 8-read average on a 1ms timer, unchanged935/800
  hysteresis and two-sample edge confirmation. No encoder IRQ or sonar polling
  competes in this diagnostic. Timing/error checks remain active.
- Existing Code39 decoder preserved. Only hand-scan capture rescales timestamps
  into8us ticks, allowing6s maximum element duration in the existing20-bit width
  storage. Reports convert back to microseconds. Quantisation is <8us; original
  edge trace keeps hardware microsecond timestamps internally and exports ms.
- Begin on white. End on decoded character, user finish,45s overall bound or
  capture error. Manual movement must remain reasonably steady: elapsed-time
  decoding still assumes wide/narrow durations reflect wide/narrow print widths.
- Store up to128 confirmed transitions (B/W and elapsed time) across decoder
  timeouts/restarts. Overflow invalidates capture and ends scan; no silent wrap.
  Two retained `[SCAN EDGES 1/2]` lines preserve the entire recorded edge list.
- No raw1kHz waveform is retained. Live analogue numbers are approximately4Hz;
  they supplement the full thresholded edge sequence and cannot identify every
  sub-threshold dip. Results cannot prove driving-speed performance.
- Shared HTTP result capacity6144B, up to7 retained lines of768B including two
  trace lines. New scan start clears old results immediately, before countdown.
- Previous movement calibration unchanged in the source/archived v5.4 file.
  This dedicated UF2 has no movement tests; a later motion build is needed to
  resume driving, after resolving the remaining optical/encoder/track issues.

## Verification

`python3 tools/test_stationary_barcode.py` compiles and runs the actual main loop
with mocked GPIO/ADC/clock/transport under ASan/UBSan. Eleven scenarios cover
slow A/Z, reversed A, USB/browser finish, GP20 finish, total timeout, element
timeout, missed-sample fault, black-start rejection, trace overflow and32-bit
clock wrap. Every simulated motor write is asserted LOW; full30-edge sequences
and reported widths checked. Old driving commands are injected and ignored.

Existing acquisition, motion, line-check and guard/diagnostic regressions pass.
Shared protocol tests run both ordinary and stationary HTTP variants; seven
maximum retained lines fit, and clearing/retention, command validation, network
handler and DHCP regressions pass. Browser mock verifies stationary-only button,
finish/download, disconnected/running lockout, reset handling and desktop/mobile.
No real robot endpoints are contacted by these tests.

Full PicoW build with -Wall -Wextra -Werror and USB movement-build compatibility
check pass. UF2 block/family/address/banner and lack of linked driving symbols
verified. Exact artifact hash, SRAM sections and source hashes are recorded in
`build/barcode-stationary/build-report-v5_5.json`. Host tests are not evidence of
physical decoding, motor electronics, optical alignment or final RTOS readiness.
