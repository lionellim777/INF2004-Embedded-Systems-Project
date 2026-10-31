# v5.1 first wireless motion evidence — 26 October 2026

Source: `serial-output/20261026-motion-v5_1-wireless-obstacle-stops.txt`
SHA-256: `d42988e2646432663bb5ac3ab1399ef6a5e20c81686ab5ffa12efaf78803bb5b`. Original download preserved byte-for-byte.
Export timestamp: 2026-10-26T11:27:36.294Z; boot ID 22665cc7.

## Operator observations (not ruler/angle measurements)

User tried all four selections. Straight travel estimated approximately 15 cm;
left/right 90 degree commands visually turned approximately 45 degrees each;
right 180 did not move. Power bank was newly carried on the robot. UI observation
fields were empty; observations were supplied in chat and remain valid qualitative
evidence, not exact calibration measurements.

## Recorded outcomes

| Selection | Actual recorded termination | Source line |
|---|---|---|
| Straight 30 cm | OBSTACLE 18.3 cm; aborted before 3537-count endpoints | 357 |
| Left 90 | OBSTACLE 25.8 cm; aborted before -994/+994 targets | 496 |
| Right 90 | OBSTACLE 25.0 cm; aborted before +929/-929 targets | 570 |
| Right 180 attempt 1 | OBSTACLE 17.7 cm immediately after START; no MOTION GO | 643–644 |
| Right 180 attempt 2 | OBSTACLE 18.1 cm immediately after START; no MOTION GO | 677–678 |

Three MOTION GO entries, five obstacle stops, zero MOTION STOP/MOTION SETTLED
completion entries. No logged motor-stall/control-delay fault or browser power-reset
marker. The captured live log begins with a gap warning, so absence claims concern
only the supplied captured interval. Encoder-invalid fields in visible run windows
remain 0/0. The recovered result at the top is a later USB/browser stop, not a
second independent run or the preceding obstacle-stop result.

Straight range was around 134 cm before motion, then 20.4 cm and 18.3 cm during
motion, and approximately 17–18 cm after stopping. The main firmware stops after
two valid sonar readings below 30 cm, including during countdowns and turns.

## Interpretation and next action

Wireless page access, selection and log download have now been exercised on the
car following the DHCP fix. Motor distance/turn accuracy and v4 end-pivot improvement
remain unvalidated: these were interrupted movements, not completed short runs.
The log proves which stop rule fired, not whether the echo was a genuine obstacle,
a nearby person, floor reflection, changed mounting, cable/bank intrusion, vibration,
or electrical interference. Extra mass may affect dynamics, but it is not established
as the cause of the reported half-distance/half-angle observations.

Preserve motor tuning and the sonar stop rule. Inspect bank/cable placement and
sonar mounting; use an open level floor with the operator clear of the sensor.
Repeat only the straight 30 cm run, watching Range. If a short distance is reported
in genuinely clear space, inspect sensor orientation/wiring and obtain a side-view
photo before changing calibration. No new firmware required for this next observation.
