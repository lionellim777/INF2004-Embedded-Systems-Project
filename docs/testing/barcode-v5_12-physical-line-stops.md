# v5.12 physical scans: line guard interrupts capture

28 October2026. Sources `serial-output/20261028-v5_12-A1.txt` and `serial-output/20261028-v5_12-Z5.txt`. Count live start/stop pairs only; retained result duplicates A. User recalls four Z attempts; second export actually contains five complete starts/stops.

A: decodedA,30 edges,1 accepted frame, BARCODE_READ stop.
Z sequence: BOTH_LINE_BLACK / READ(Z) / BOTH_LINE_BLACK / BOTH_LINE_BLACK / BOTH_LINE_BLACK. Interrupted scans contain23,23,23,24 barcode edges (22/23 measured widths), zero completed frames, zero decoder rejections and zero capture timeouts. AcceptedZ has30 edges. All runs have zero encoder-invalid and sampler integrity/overflow faults. Both central line sensors triggered the stop; this is independent of the offset barcode channel. Sparse live reports may show one sensor cleared after the stop.

The rejected-width issue is not exercised by these interrupted runs: the decoder never receives a complete29-run frame. Both successfully completed frames decode correctly. Source guard in barcode_drive_step immediately stops when ir.left && ir.right, before barcode_capture_stop. A stop already raised there takes precedence over a valid decode. No drop-off detection exists. Do not infer which physical surface caused both-black solely from logs.

## Calibration versus mission

Current moving Barcode pass is explicitly a bounded single-frame diagnostic: stop on valid read, rejected frame, element timeout, both central sensors black, obstacle or other fault,8s or8000counts. It logs record-only with no navigation action. This is not the final continuous assessment behaviour the user now emphasizes.

The reader expects one Code39 A-Z character between asterisk guards:3 symbols of9 alternating elements plus2 white separators=29 runs/30 transitions. This is a fixed format assumption, not a preselected A/Z answer. Letter patterns A-Z are a symbol dictionary. A different course payload length/encoding would need parser changes. Code navigation mapping A/B/C/D exists but these diagnostics do not execute it, and mapping must be checked against the assessment brief before integration.

Next implementation work is the line/mission state handling and a distinct continuous integration mode. Valid barcode events must be recorded/consumed and capture rearmed without a diagnostic stop each time. Recoverable failed scans require a mission-aware policy rather than treating every reject as a motor fault. Both-black needs bounded handling informed by track/junction geometry, not an unconditional stop or blind removal. Preserve real abort/fault protection and the existing diagnostic mode. This analysis makes no firmware change and does not claim course readiness.
