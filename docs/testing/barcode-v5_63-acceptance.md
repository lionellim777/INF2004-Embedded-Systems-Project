# v5.63 barcode decoder acceptance —7 November2026

Status: implementation complete and physically validated for the current home
hardware/print/track setup. Freeze v5.63 as the working barcode baseline.

Source: `logs/20261107-v5_63-two-AAZ-runs.txt`, copied unchanged from
`AAI10-motion-2026-11-06T16-31-12-319Z.txt`. The user reports two physical runs
and all three barcodes reached/read in each, including half-Z on the right bend.
The export contains two distinct event sets and capture timestamp ranges;
repeated retained summaries are not counted as further runs.

| Run | Full-A event ms | Half-A event ms | Half-Z event ms | Sequence | End |
| --- | --- | --- | --- | --- | --- |
|1|9517|19575|32507|AAZ|OBSTACLE_STOP|
|2|8337|18367|30975|AAZ|OBSTACLE_STOP|

The globally later run has captures starting188155782us for full-A and
211383782us for Z; the earlier run starts48950782us and73066782us.
Event timestamps above are relative to each run's motion start, not global time.
The two event sets are definitive; telemetry/retained-result order is mixed.
For clarity, the run labels in the table identify event sets rather than infer
ordering from where a repeated result appears in the text.

All target reads:6/6 over2/2 observed AAZ passes. No extra accepted letters,
no integrity/event overflow, no waveform timing/queue/buffer overflow or
ambiguity. One/two rejected intermediate captures do not mean target misses:
the accepted event sequences contain exactly the expected AAZ.

The two Z captures report64mm/20deg and88mm/25deg lateral/yaw excursions,
respectively. This supports scanning during turning with the user's observation;
it does not reconstruct an exact camera-observed trajectory. Geometry candidate
masks appear on both half-A scans and on one Z; the existing strict path also
remains available. Both runs have21 successful line recoveries overall.

Observed foreground waveform max_work_us71037/70022 and max_loop_us80760/82161
are below the150ms service guard in these runs. This is run-scoped evidence,
not a worst-case execution bound or stack high-water measurement.

The raw waveform export ends `[WAVEFORM EXPORT INCOMPLETE] Failed to fetch`.
Therefore this record verifies accepted events/telemetry, not a new raw-waveform
checksum or frozen-signal replay. No additional physical run is needed merely
to establish the six accepted events already present.

Acceptance scope: GP26 barcode at80mm forward/85mm left, GP28 centre/GP27 right,
18mm tape, GP27 spacing20mm, full6/18mm and half3/9mm bars/gaps, flat side barcode
at the right bend. This is not a universal100% reliability claim, physical
coverage of every A-Z symbol, all directions/print sizes/lighting, or proof of
barcode-triggered turns/junctions. A events are intentionally unmapped in this
mission; Z is configured Straight. Obstacle bypass mode still disables barcodes.

Keep barcode implementation closed unless a reproducible regression appears.
Final mission/RTOS/telemetry/navigation integration and a final course regression
remain separate work. Preserve the exact v5.63 source and UF2 hash; do not retune
its barcode thresholds solely to chase zero rejected noise fragments.
