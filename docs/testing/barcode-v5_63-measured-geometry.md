# v5.63: measured print geometry and expanding bounded recovery

## Physical information supplied on7 November2026

Half barcode is the complete symbol with every bar AND white gap halved in
width. It is not a cropped symbol. Half: narrow3mm, wide9mm; full: narrow6mm,
wide18mm. Black and white have the same3:1 nominal ratio. Reported bar height
is approximately260mm for both sizes. Z is a flat rectangle beside the bend,
not a print whose stripes follow the curve. Route tape is18mm wide; GP27's
sensing spot is20mm right of GP28. Existing GP26 geometry remains80mm forward,
85mm left of the powered-wheel midpoint. No GP28 fore/aft measurement changed.

Height does not change the symbol in a straight one-dimensional scan. It does
matter when the sensor shifts along the bars during recovery. The old3mm
lateral-return rule incorrectly used a narrow-bar width as a transverse limit.
The firmware now logs that shift instead of declaring the frame invalid solely
for crossing another lateral row. It does not assume the sensor starts centred
within the reported bar height. Missing stripes still cannot be reconstructed.
A photo of the barcode endpoints relative to the line sensors remains useful:
long bars extending beneath GP28/GP27 could produce unwanted line readings.
That overlap is a possibility to inspect, not established by these logs.

For ideal point sensors, an18mm line and20mm sensor spacing leave a2mm interval
between centre-sensor loss and right-sensor acquisition on a rightward edge.
Finite optical footprints affect that interval. Both-white does not uniquely
identify bend direction, so widening a bounded search is preferable to granting
an unsupported hard turn from direction history alone.

## Reader changes

- Retain the old strict decoder and v5.62 bounded-width candidate method.
- Add an encoder-domain model of the15 BLACK-BAR CENTRES. For a flat print,
  an additive edge bias that shortens black and lengthens white approximately
  cancels at the bar centres. This handles faint narrow bars without relaxing
  the print's nominal3:1 white/black geometry.
- Enumerate all A-Z and both symbol orientations. Require correct black-width
  ranks for both star guards and the payload. Fit all15 centres to each3:1
  template with one scale and one offset. No expected A/Z hint is supplied.
  Narrow scale must be within25% of measured3mm or6mm under existing nominal
  wheel60mm/2543CPR. Every centre residual and every adjacent-centre spacing
  residual must be <=0.65 narrow modules. These are explicit provisional
  geometric tolerances, not calibrated optical uncertainty.
- Require at least two prominence profiles to agree for this new fallback;
  conflicting candidates reject. Sliding windows may contain a complete frame
  after prefix activity; no stripes are inserted or removed.
- Separate the raw apparent-width floor from the old8ms time-domain floor.
  Motion input uses4mean counts (~0.296mm nominal apparent width); time-only
  input retains8ms. The3mm physical narrow module is validated by the fitted
  geometry, not by assuming its thresholded black appearance is also3mm.
- Retain all29 widths in a short-run failure diagnostic instead of zeroing
  the candidate. Log geometry_mask and lateral_recrosses in addition to the
  existing per-attempt and frozen-waveform diagnostics.
- Guard/black-rank prefiltering avoids52 full geometric fits on unrelated noise.
  Move reusable decoder/drain scratch buffers off the stack; retained main
  controller objects use static storage. Reserve4KiB for the main stack.
  Runtime maximum decoder work still needs measurement on the Pico.

## Recovery changes

Flow-follow, ordinary line loss only: after initial120-count and opposite
240-count sweeps, permit two wider alternating sweeps of480 and720 counts.
Because each sweep starts at the prior endpoint, total endpoints are roughly
+/-360counts from initial orientation (~+/-27deg under nominal geometry).
The overall3s deadline, per-stage1.2s limit, encoder stall/wrong-direction,
coast, control and motor guard checks stay in force. There is no unlimited
retry and no extra blind forward motion. The larger sweep may not complete
before the overall deadline if the car turns slowly.

Legacy/obstacle and commanded-turn searches keep their earlier policy. The
existing timed live-right-preview gate for rolling arcs remains. wide_searches
counts the wider-sweep transitions. This expands the recovery envelope; it is
not a guarantee of finding a line outside that envelope.

## Evidence

The three archived failed v5.62 snapshots have verified FNV checksums. Current
reader batch replay is asserted to return A / Z / Z respectively: half-A from
run1 and half-Z from runs2/3. This is not the three full-course result sequence.
Actual v5.62 full-course sequences were empty / A / A. Run1's invalidated full-A
raw waveform was not retained, so its improved outcome cannot be replayed.

New tests cover104 A-Z/orientation/print-size templates with large optical edge
bias, expanded interior elements and510 invalid guard pairs; a simulated10mm
lateral recross within long bars; and leaving a simulated print before the final
guard. Existing contrast/noise/quiet/motion suites remain. Recovery tests cover
finding tape beyond the old sweeps, exhausting both wider sweeps and retaining
legacy bounds. Actual-main, obstacle, wireless and browser checks are recorded
in the release JSON. Frozen replay is not a physical v5.63 read or a proof of
false-positive rate. The mean-count geometry, scan-axis alignment and tolerance
selection still need physical tests; unrelated noise could resemble a code.

## Next run

Flash `firmware/releases/robocar_motion_integration_v5_63_wireless.uf2`, check
v5.63 in the page/log, leave both Y/Z actions Straight, and run the unchanged
full-A -> half-A -> right-bend half-Z route once. Download before restarting.
Target sequence is AAZ. Inspect geometry_mask, lateral_recrosses, wide_searches,
max_work_us and the final stop reason. Preserve the layout while comparing runs.
No100% bend or barcode claim is made; wheel slip, missing stripes, bad alignment
and unobservable line direction can still prevent a read or reacquisition.
