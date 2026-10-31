# v5.2 physical right90 and IR pass — 26 October 2026

Evidence: `serial-output/20261026-motion-v5_2-right90-ir-pass.txt`.
SHA-256: `0c6d6a4d584c5ba108c6e3a4175ba5033364543bd1598dae5b6bd6df489a56ba`.
Boot 03560106. Recovered straight result duplicates live lines; count it once.
The capture contains three log-gap markers, so absence of other events is not a
complete-session guarantee.

User confirms right90 now reaches the intended angle. Retain the 987/-987 target;
this is an operator observation, not a protractor measurement. Right90 completed:
cutoff 987/-995, settled 1074/-1068, invalid transitions 0/0. Recorded maximum loop
36.943 ms, Wi-Fi poll 2.344 ms. No additional turn tuning justified.

Straight completed: target 3537/3537, cutoff 3544/3538, settled 3612/3592, invalid
0/0, peak skew 49. This log does not itself establish measured ground distance.

| Logged IR channel | Raw minimum–maximum | Threshold transitions |
|---|---|---|
| L / GP26 | 178–205 | 0 |
| C / GP28 | 172–3199 | 5 |
| R / GP27 | 143–164 | 0 |

Only C showed strong contrast during this straight pass. Five threshold transitions
are not five barcode bars. The final C reading remained high. User placed the black
strip between the two central line sensors; exact strip orientation and individual
coverage were not established. Low unchanged values do not prove a failed sensor.

Other simultaneous evidence: 14 valid sonar samples, 5 no echoes, valid range
70.1–93.0 cm; 93 fresh pitch samples spanning -3.3 to +7.0 degrees, zero new humps
and zero IMU read errors. A climbing candidate started then cancelled for no descent;
acceleration or handling may contribute, but cause is not established. Maximum loop
36.718 ms and Wi-Fi poll 2.550 ms during this run. These are observed maxima, not
worst-case guarantees or a full sonar/terrain calibration.

## Physical-role correction and next step

User explicitly confirms two central sensors for line-following and one offset
sensor exclusively for barcodes. Earlier all-three-across-tape instructions did
not account for this layout. Existing L/C/R names identify wired channels; do not
infer their current physical roles from the names alone.

Current source still uses all three channels for line visibility, L minus R for
steering and a two-of-three black vote for barcode input. That does not implement
the user's dedicated barcode layout. Encoder-selected diagnostics do not use this
line-following steering or barcode mode; keep those calibration targets unchanged.

Next: stationary identification with motor power off, Pico/browser powered, no
GP20. Put black beneath the central-left sensor, central-right sensor, then offset
barcode sensor individually, holding each for about 3 seconds with white between.
Keep each sensor at its normal floor height. Download the log and report this order;
match changes in IR=a/b/c to GP26/GP28/GP27 before remapping the two-sensor steering
and dedicated barcode input. This is a role/contrast check, not a restart of motor
or sonar calibration. No firmware change or threshold change made from this log.
