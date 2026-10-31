# Stationary paper sweep — 28 October 2026

Source: serial-output/20261028-v5_4-stationary-paper-sweep.txt.
User slowly slid paper rather than holding named regions; reports indicator fails
to light over narrow white gaps although it lights over white generally.

273 live IR rows, all stopped; no completed motion or high-rate barcode result.
GP26 first channel spans186–2743, GP28 170–196, GP27 137–164. This supports
channel identity and strong overall contrast. It cannot identify which numeric
values belong to the particular narrow gap: positions were not labelled, and
idle text snapshots are roughly250 ms apart, not a retained1 kHz waveform.
Seven high regions appear in this log; that is not a reliable bar count.

LED behaviour alone does not prove a firmware threshold crossing: firmware uses
analogue GP26, not the LED or digital output. Current barcode hysteresis is ON935,
OFF800. Optical footprint/height, alignment and analogue thresholds remain
possible causes of merged gaps; no confirmed cause yet.

Next test: car stays stopped. Centre a narrow gap beneath optical tip and bring
paper slightly closer while keeping flat. Compare raw first value against a
black bar at same height. Need gap reliably <=800 and black >=935 for current
settings; if separable but both above800, consider software threshold calibration
using labelled measurements rather than assuming sensor is incapable. Do not
change polling or thresholds yet. No moving test on short raised track.
