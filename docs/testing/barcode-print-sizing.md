# Supplied barcode PDF and track photo — 26 October 2026

Source: user's Barcode Sample.pdf, four 960 x 540 point slides (338.67 x 190.5 mm).
Pages 1/2 are Code39 *A*, pages 3/4 are *Z*. Vector bars match the existing decoder
patterns 0x094 / 0x109 or 0x0d0 / 0x094, with 29 alternating runs and 3:1 wide/narrow.
At native PDF scale, first-to-last bar spans approximately 328.50 mm on pages 1/3
and 164.94/164.90 mm on pages 2/4. No required physical print size is stated.
The track photo shows large and small side barcodes and route crossings, but has no
reliable dimensional reference; do not infer official course dimensions from it.

Generated practice file: output/pdf/AAI10-barcode-calibration-A4.pdf, four A4 landscape
pages, *A* then *Z*, each large/small. Large bar span235 mm, narrow5 mm/wide15 mm;
small117.5 mm, narrow2.5 mm/wide7.5 mm. All bars60 mm tall. These are explicitly
chosen home-calibration dimensions, not a course requirement. Geometry along the
scan path preserves the supplied symbol patterns. Labels are moved outside the
scanning band; the original full-width pages put a letter in a white gap that could
create spurious transitions if the sensor passes over it. Each sheet includes a
100 mm ruler and instructions to print Actual size100%, one A4 landscape page/sheet.
Vector spans and decoded patterns checked on all four outputs; all pages rendered
and visually inspected. Original source untouched. Start with output page1 (*A*).

Mount so offset GP26 traverses the bars/spaces while GP28/GP27 straddle the main
route line. Bar lengths run across the direction of travel; retain white areas at
both ends and keep tape/text out of the sensor's path. The whole barcode need not
fit beneath the car simultaneously. Exact transverse offset should be set from the
actual sensor placement, not guessed from the photo.

Firmware implications (not implemented in this print-only turn): native large bar
span328.5 mm already exceeds the nominal30cm diagnostic; allow entry/exit margins
and complete capture before ending a dedicated barcode test. At representative
200 mm/s, 5mm narrow feature lasts25ms; 2.5mm lasts12.5ms. Prior observed loop gaps
up to38.5ms can miss them. These are illustrative timing calculations, not measured
speed or a supported speed limit. Barcode sampling must be made sufficiently frequent
or run at a measured, justified speed before claiming reliable moving decoding.
Do not disable sonar protection just to improve capture. Keep decoded symbols
record-only during calibration. Decoder accepts Z, but navigation maps only A/B/C/D;
Z has no assigned action and must not be assumed to mean STOP or an error in reading.
Crossings in the track photo require deliberate junction policy: v5.3's both-black
stop is a diagnostic rule, not final-course behaviour.

User clarified previous runs involved repositioning and rolling off a foam platform.
Those logs are not a controlled level-ground terrain validation, and the large pitch
excursions cannot be labelled acceleration artefacts without better evidence.
