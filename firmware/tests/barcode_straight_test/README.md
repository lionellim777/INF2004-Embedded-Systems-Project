# Barcode straight-drive test

Tests barcode reading on its own, without line following. The car drives
straight (default 400 mm) at the course scanning speed, about 44 mm/s
(600 counts/s per wheel), while the GP26 sensor reads a barcode placed beside
its path. It uses the same sampling, filter, reader and decoder
as the course firmware; with the `course` levels, a pass or fail here
applies to the course too.

Not used: line sensors (GP27/GP28), sonar, servo, IMU, Wi-Fi. The
black/white levels can differ from the course firmware; see below.

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
4. The default 400 mm covers lead-in, either barcode size and a little after.
   Leave about 50 cm of clear space in front of the car.

## Run

USB is optional during runs; the car can run on its battery alone.

1. Press **GP20**. The car waits 3 s, then drives and stops.
2. Reposition and repeat. Up to 20 runs are kept in flash and survive switching off.
3. Plug USB back in (switching off in between is fine): every stored run is
   printed with a count of runs that read a letter. A run is saved only when
   the car stops by itself, so do not switch off while it is moving.
4. Serial keys: `r` all stored runs, `e` every black/white change of the
   last run, `o` raw sensor trace of the last run (both saved in flash, so
   they survive the restart when USB is plugged in), `l` switch black/white
   levels, `c` clear, `3`/`4`/`5` select 300/400/500 mm, `x` abort, `?` help.
   GP20 also aborts a run.

## Black/white levels

`l` cycles through three settings; the choice is kept in flash and each run
records the levels it used (`levels=on/off white=`).

- `course`: the course firmware's fixed 226/220.
- `mid`: fixed 1600/1400.
- `auto` (default): during the 3 s countdown the car measures the white
  paper under GP26 (median of 31 readings) and uses white + 850 / white + 650.
  If the reading is not steady, the run does not start; keep the car still
  on white and press GP20 again.

The auto margins come from raw traces on this car: white stayed within about
+550 of its resting value, and the faintest narrow bar of the small barcode
peaked about +1260 above it.

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
