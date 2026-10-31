# v5.3 line-check physical results — 26 October 2026

Raw evidence: `serial-output/20261026-v5_3-line-check-runs.txt`.
SHA-256: 292e1a6d02d61e9b42a2f86586eed93e350d880b1d9343187411412a2f4a893e. Boot 84b365d0. Recovered result duplicates the final live run;
count it once. Seven distinct starts/stops/check summaries occur in the live log.

User observes correction back toward the line from a slightly angled start, describes
steering as somewhat wonky, and reports stopping on a perpendicular approach.

| Run | Stop reason | Settled left/right | Invalid transitions |
|---|---|---|---|
| 1 | LINE_TRAVEL_LIMIT | 3672 / 3619 | 0 / 0 |
| 2 | LINE_TRAVEL_LIMIT | 3683 / 3253 | 0 / 0 |
| 3 | LINE_TRAVEL_LIMIT | 3692 / 2994 | 0 / 0 |
| 4 | LINE_BOTH_BLACK_STOP | 122 / 130 | 0 / 0 |
| 5 | LINE_TRAVEL_LIMIT | 3731 / 3554 | 0 / 0 |
| 6 | LINE_TRAVEL_LIMIT | 3684 / 3529 | 0 / 0 |
| 7 | LINE_TRAVEL_LIMIT | 3688 / 3268 | 0 / 0 |

Both-black run stopped at 75/81 counts before settling; this agrees with the
intentional diagnostic rule and is consistent with the reported perpendicular
approach. It is not implemented junction/reacquisition behaviour. Six travel limits
are normal endpoints for this test, not stalls or proof of measured 30 cm travel.
Left/right count differences during steering are expected and are not grounds for
changing wheel CPR or the successful straight/turn calibration.

Current control switches between 40/40 percent PWM and 25/55 or 55/25 percent.
This is coarse diagnostic steering. The user observation and electrical response
support correct role mapping and initial correction, not smooth continuous tracking.
Threshold-crossing counts alone do not separate genuine recrossings from noise.
No gain, threshold or motor endpoint changes made from this log.

No completed new humps or IMU read errors were recorded in any run. Pitch extrema
across check summaries range from -16.5 to +9.3 degrees. Climb/descent candidates
occurred. Whether the surface was flat and the car untouched is awaiting the user;
acceleration-related apparent tilt is a possibility, not an established cause.
Terrain state can persist across separate test runs; a short zero-event summary does
not validate long-run false-positive behaviour or hump height accuracy.

Maximum logged loop gap: 38.476 ms. Maximum per-run Wi-Fi poll: 2.416 ms.
Some runs contain many no-echo readings (13/14 and 11/14); this is missing range
information, not a verified clear path. None of these runs reports a sonar stop.

Next: no more arbitrary-angle sensor-role checks. Obtain the actual barcode strip
(or its course specification) for a dedicated barcode capture check, while using
flat-floor motion as the terrain false-trigger baseline. Follow with real-hump and
obstacle-scan/recovery checks. Smooth line tracking, line-loss/junction behaviour,
full-speed barcode capture, terrain measurement, RTOS and mission integration remain
open. The bounded v5.3 diagnostic is not the final autonomous controller.
