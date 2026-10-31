# v5.9 physical A and Z success — 28 October 2026

User-labelled one A pass and one Z pass, both boot71a7c9ad. Local evidence:
`serial-output/20261028-v5_9-A-success.txt` and
`serial-output/20261028-v5_9-Z-success.txt`. Recovered sections repeat the live
results; count exactly one run per file.

| Run | Decode | Samples | Edges/frames/rejected | Settled L/R | Encoder invalid | Max sampling gap |
|---|---|---|---|---|---|---|
| A | A valid1 |1559|30/1/0|3675/3684|0/0|1022us|
| Z | Z valid1 |1614|30/1/0|3908/3937|0/0|1015us|

Both stopped automatically on BARCODE_READ, with no timeouts, overflow, gap,
integrity, trace or stale faults. This is physical recognition of both symbols
on the current print/setup, not a navigation action or whole-course acceptance.
The generic CHECK IR_L edges12 remains the old normalised IR diagnostic; the
dedicated barcode acquisition result is30 edges and is authoritative here.

Line response: the A live log shows BARCODE_CORRECT_LEFT with PWM0.25/0.55,
then returns to BARCODE_SPEED_HOLD with central-left white. Central-left reports
8 transitions in each run. No right-line transitions were observed; rightward
recovery has not been exercised by this evidence. Z's sparse live rows do not
show a steering-state row, although its aggregate records left-line edges.
User visual confirmation is still required to call the following behaviour
physically satisfactory; do not infer perfect tracking from the decode alone.

Nominal1200cps speed tracking remains unvalidated: live running wheel speeds
commonly exceed2200cps, with peaks near2746cps. These successful decodes are at
the actual observed speed, not proof of9cm/s regulation. No controller change
was made in this analysis. Sonar/terrain were logged but these runs do not
validate obstacle stopping or hump accuracy.

Keep v5.9 for the planned initial repeatability check. Current-version tally is
A1/3 and Z1/3; two more successful passes each would reach the previously stated
three-per-symbol local screen. Older v5.8 A successes remain historical, not
substitutes for changed-steering-version repeatability. Different barcode sizes,
track geometries and full mission behaviour remain separate tests.
