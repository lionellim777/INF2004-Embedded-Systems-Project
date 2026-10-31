# v5.9 obstacle-stop physical result — 28 October 2026

User observed stopping approximately20–25cm before the obstacle. In the preceding
turn the user explicitly confirmed that v5.9 followed the strip.
Source: serial-output/20261028-v5_9-obstacle-stop-success.txt, boot71a7c9ad.
One actual run; recovered result duplicates live result.

Stop reason: OBSTACLE_STOP, printed sonar26.7cm. Prior live reading29.8cm.
Current source requires two consecutive valid sonar measurements below30cm;
sonar polls are scheduled100ms after each measurement finishes. Thus30cm is a
trigger threshold, not a promised stationary clearance. After the stop, reported
sonar is roughly25–26.3cm and encoder counts settle unchanged at raw-12754/12786.
Measured distance reference/accuracy were not supplied; do not calibrate sonar
scale against the user's approximate20–25cm observation.

Settled relative counts5171/5164, invalid0/0, no barcode capture faults,1 edge,
no decoded symbol (expected with barcode removed). It stopped before8s/8000
limits; active capture2140samples. Sonar25 valid/0 no-echo across diagnostic
window. Aggregate distance25.0–210.1cm includes outlying background echoes:
one moving live reading jumps44.9->141.7->34.6cm. This successful stop is not
proof of calibrated sonar accuracy or reliability for all target geometries.
HUMP descending message/pitch excursion after braking is not a validated hump.

Conclusion: one successful integrated line/obstacle-stop demonstration on this
setup. No firmware changed. Speed tracking remains open: actual moving rates
roughly2300–2700cps versus nominal1200. Next development priority is resolving
that tracking error while preserving confirmed barcode and steering behaviours;
servo scanning/bypass remain separate unimplemented diagnostic steps. No need
for another identical obstacle pass solely to interpret this log.
