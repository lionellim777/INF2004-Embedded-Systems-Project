# v5.2 right-90 trim and combined checks — 26 October 2026

## Supplied evidence

Raw export preserved as `serial-output/20261026-motion-v5_1-completed-turns.txt`.
SHA-256: `0b053c29594956c9c6685d8212ba34524550873774cffd4dcb2ba7f7f396b29e`.
This browser log includes the previous obstacle-aborted session, then a power-reset
marker at line 863 and subsequent completed turns (boot 943348f8). Do not count the
old aborted moves as new completed runs. Both startup log windows include gap markers.

User observations in chat: straight 30 cm okay; left 90 solid; right 90 approximately
85 degrees; right 180 perfect. Angles are visual estimates, not protractor measurements.
The supplied capture contains no new completed straight run; retain the straight
assessment as operator-reported, not independently established by completion lines.

| New completed movement | Settled L/R | Cutoff L/R | Invalid | Max loop / max Wi-Fi poll (microseconds) |
|---|---|---|---|---|
| Left 90 | -1060 / 1069 | -1002 / 998 | 0 / 0 | 35344 / 4008 |
| Right 90 #1 | 1008 / -974 | 933 / -931 | 0 / 0 | 35340 / 4673 |
| Right 180 #1 | 2163 / -2144 | 2090 / -2088 | 0 / 0 | 35375 / 1001632 |
| Right 180 #2 | 2170 / -2140 | 2094 / -2094 | 0 / 0 | 36155 / 1001632 |
| Right 90 #2 | 1005 / -987 | 931 / -934 | 0 / 0 | 14670 / 1001632 |

In v5.1 the Wi-Fi maximum was since boot, not per run. The logged CYW43 stall/send
failure at lines 1105–1106 occurred while idle before the R180 runs; the later
one-second maxima must not be represented as a measured one-second moving-loop
stall. However a foreground radio stall can recur, so independent motor cutoff is
added. The radio driver's underlying stall cause is not claimed resolved.

## Narrow tuning change

R90 mean settled magnitudes: (1008+974)/2=991 and (1005+987)/2=996.
Their mean is 993.5. Using the user's approximate 85-degree endpoint:
additional counts = round(993.5 * (90-85) / 85) = 58.
R90 cutoff increases from 929 to **987** counts per wheel, preserving signs.
This is a provisional empirical trim, assuming comparable surface, placement and
residual rolling; it does not establish a general counts-per-degree relationship.
Straight3537, L90-994/+994, R180+2087/-2087, CPR2543, PI values and wheel/track
geometry remain unchanged. Only the next measured/observed R90 run can validate it.

## Diagnostics added to each selected movement

- `[MOTION SETTLED]` retains per-wheel endpoint/coasting/error information.
- `[CHECK]` aggregates IR raw minima/maxima and black/white transition counts for
  each physical channel; fresh sonar valid/no-echo counts and valid range extrema;
  fresh filtered accelerometer pitch extrema and sample count; new hump events and
  IMU read failures; explicit outcome. Sampling covers motion and 500 ms settling.
- Zero pitch/sonar sample count means unavailable, not a measured zero reading.
- IR transitions use existing thresholds; they reveal response/placement but do
  not auto-write thresholds, establish side-barcode geometry, or prove decoding.
- Pitch from accelerometer-only GY511 includes acceleration effects. Level-floor
  runs expose false hump candidates; they do not calibrate actual hump height.
- Wi-Fi maximum is reset for each GP20-started run; loop maximum already resets.
- Stop, settled, check and timing lines survive rolling-log overwrite in RAM and
  are included by Recover last result / Download log. New moves/power resets clear
  retained results. UI feedback fields removed; physical observations go in chat.

## Independent motor service guard

All drive requests pass through one guarded function. A repeating alarm checks
every 10 ms; if nonzero drive has not been refreshed for 150 ms it removes PWM
and latches a fault. It does not print or call network functions in the interrupt.
Main refresh/reset and both wheel writes use a short interrupt exclusion. A latched
fault rejects drive until GP20 explicitly starts a new run; alarm allocation failure
blocks starts. Delayed Wi-Fi returns cannot automatically resume PWM. This is a
software timer guard, not a certified emergency stop; long IRQ exclusion and hardware
faults remain outside its guarantee. On-car cutoff latency still requires validation.

## Next on-car sequence

1. Flash v5.2, rejoin AAI10-Calibrate (same password), refresh the page and confirm
   v5.2. Keep the power bank secured in the same position as these successful runs.
2. Select Right90 and press GP20. Report whether it now reaches a right angle.
3. On level light-coloured floor, place a flat matte-black strip about 3 cm thick
   across the path, about 10 cm ahead of the front IR sensors, wide enough for all
   three sensors. Keep the sonar's view clear. Select 30 cm forward and press GP20.
4. Download the log after `[CHECK]` and `[TIMING]`. Report distance, straightness,
   R90 appearance and whether all three sensors physically crossed the strip.

This single straight pass combines motion, line-sensor contrast/placement, sonar
stability, pitch/hump-noise and scheduling checks. It is an evidence-gathering next
stage, not automatic full-system calibration. No unattended chain of moves is added.

## Software verification

Motion target/PI/fault tests, diagnostic freshness/aggregation tests, guard timeout,
latch, idle and wrap tests, and wireless protocol/result tests run under ASan/UBSan.
The browser is tested with mocked responses for form removal/download and reconnect.
Full Pico W wireless and USB-only builds passed with warnings treated as errors.
UF2 block structure, RP2040 family, flash addresses and v5.2 banner passed verification.
The desktop/mobile mock browser renders were inspected. Hardware results remain pending.
Artifact: `build/motion-wireless/robocar_motion_integration_v5_2_wireless.uf2`
(826368 bytes; SHA-256 `59ff923c15fc9504508798451de31816e900f9029df4ea1db1e9dcd1f90a8c7d`).
