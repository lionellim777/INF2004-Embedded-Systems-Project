# Barcode straight-drive test

Tests barcode reading on its own, without line following. The car drives
straight (default 300 mm) at the course scanning speed, about 44 mm/s
(600 counts/s per wheel), while the GP26 sensor reads a barcode placed beside
its path. It uses the same sampling, filter, thresholds, reader and decoder
as the course firmware, so a pass or fail here applies to the course too.

Not used: line sensors (GP27/GP28), sonar, servo, IMU, Wi-Fi.

## Build

```sh
cmake -S firmware/tests/barcode_straight_test -B build-barcode-straight -G Ninja -DPICO_BOARD=pico_w
cmake --build build-barcode-straight
```

Needs Pico SDK 2.1.1 or newer. Flash `barcode_straight_test.uf2`.

## Set up

1. Lay the barcode flat on white paper. Bars must run **across** the direction
   of travel, so the car drives along the barcode's length.
2. Line the car up so **GP26** (the offset barcode sensor) will pass over the
   bars, and point the car straight along the barcode.
3. Put GP26 on white **at least 30 mm before the first bar**. Reading starts
   only after 250 ms of white. Leave white after the last bar too.
4. Choose a distance that covers lead-in + barcode + a little after:
   - half size (about 141 mm): 300 mm (default)
   - full size: 300 mm for a 235 mm print, 400 mm (press `4`) for a
     328 mm print

## Run

1. Connect USB and open a serial monitor.
2. Press **GP20**. The car waits 3 s, then drives.
3. GP20 or `x` aborts. `3`/`4`/`5` select 300/400/500 mm, `r` reprints the
   last result, `?` prints help.

## Reading the output

- `[BARCODE EVENT] letter=A value=1 command=LEFT` means a letter was read.
- `[BARCODE RETRY] rejected=1 ...` means 29 bars/spaces were captured but did
  not form a valid letter (`value=6`, unreadable). The listed widths show
  what the sensor saw.
- `[RESULT] reads=… sequence=… nominal_mm=…` summarises the run. Measure the
  actual distance travelled with a ruler; the nominal figure uses the wheel
  size and has not been calibrated at this speed.
- `[OPTICAL DATA]` lists raw GP26 minimum:maximum per 10 ms, for 5 s from the
  first dark edge. Save the whole log; it shows whether narrow bars were seen.

Suggested record per run: barcode size, letter, distance, measured travel,
`reads`, `sequence`, `rejected`, `raw` range. Target: each letter read in at
least 9 of 10 passes.
