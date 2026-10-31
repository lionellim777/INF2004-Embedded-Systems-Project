# v5.10 speed and obstacle run — 28 October 2026

Source: `serial-output/20261028-v5_10-speed-obstacle.txt`.
User measured approximately33cm from starting to ending wheel-centre position, stopping before a bottle. This is travel, not final obstacle clearance; visual smoothness was not explicitly reported.

The export contains old v5.9 logs and a stale v5.9 browser heading. The v5.10 boot banner at source line410 and current retained result identify the new run; do not aggregate old runs. Refresh the browser before the next export.

- Stop: OBSTACLE27.2cm, settled sonar approximately26.8cm. These are sensor readings, not ruler-verified clearance.
- Clear-interval mean wheel rates:1172/1155cps against1200, errors -2.33%/-3.75%,29 intervals after0.5s. Ranges1048..1301 and991..1426cps. Five steering intervals excluded.
- Settled counts4822/4838; invalid transitions0/0. Counts are not converted to distance or used to overwrite existing calibration from this single run.
- Barcode samples4134; maximum gap1027us; no overflow/gap/integrity/staleness faults. No barcode edges/frames expected with barcode removed.
- Left central IR12 transitions; right0. Line corrections occurred between the sparse live reports. This alone does not certify whole-track line following.
- Pitch -2.6..2.6degrees, no IMU errors. No hump validation implied.
- Sonar has implausible jumps to144–189cm during approach. Bottle geometry/background returns are a possible explanation, not proven; small-object reliability remains open. Do not silently treat those as verified clear space.

Conclusion: new controller demonstrated near-target mean straight-running rates in one physical run, with obstacle stopping. Keep v5.10 for moving A then Z regression at the revised speed; no firmware change required for these. Refresh browser first, remove bottle from the scan path, retain a continuous safe surface and GP20 abort. Subsequent sonar checks should include a broad flat target as well as the bottle.
