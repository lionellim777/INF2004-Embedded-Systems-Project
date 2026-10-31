# v5.12 — colour-width bias correction

28 October2026. Built, not flashed; physical A/Z confirmation pending.

## Evidence and method

v5.11 Z1 and A2 each captured all30 edges but failed the old shared-width1.8 ratio test. Their narrow white durations were longer than narrow black durations. This supports investigating measurement bias; it does not prove a particular optical or mechanical cause.

For the supported A-Z and asterisk patterns, each9-element symbol contains two wide black bars and one wide white gap. Sort each colour separately to nominate the wide elements; ties at each split reject. Each colour has three narrow runs. Let B and W be their respective medians. In doubled units the corrected black width is2*raw+(W-B), white is2*raw-(W-B). This estimates a symmetric additive edge-crossing bias without floating point or external calibration. Correction is local to each symbol; no expected letter is supplied to firmware.

Ranks alone cannot accept a symbol. Corrected widths must be positive, minimum wide/maximum narrow must remain>=1.8, and the maximum/minimum within each corrected class must be<=2. The latter is a conservative plausibility guard, not a claim of formal barcode-standard conformance. Exact asterisk guards, supported letter pattern and nonzero/narrow separators checked against both adjacent white thresholds remain required. Raw widths outside1..750000 are rejected consistently by array and packed paths. The algorithm cannot recover missing edges, clipped paper scans or arbitrary invalid patterns, and Code39 without a checksum cannot guarantee rejection of all corruption that produces another valid symbol.

## Verification

Nine frozen complete v5.10/v5.11 recordings replay as their user-labelled A/Z, including both newly rejected v5.11 frames. All also decode when reversed; through the moving capture path they request BARCODE_READ and zero output. Older v5.10 A2 distorted evidence remains rejected by the coherence guard; incomplete Z capture still times out. No blanket relaxation of decoder outcomes.

Tests cover all26 letters with additive biases -40,-20,0,20,40 relative to100-unit narrow widths, both directions; all512 candidate data masks and512 guard masks; zero and oversized elements at every position; white separator corruption; uniform ambiguous runs; weak1.7 separation, and acceptance/rejection at1.8/1.79. Array/packed equivalence, storage, Cortex-M0+ layout, moving/stationary capture, steering, calibrated motion, guard and wireless regressions passed. Replays and synthetic tests do not establish a physical false-read rate.

The old packed-storage test used a historical decoder as an exact acceptance oracle. That is no longer appropriate after an intentional decoding change: it now compares current array and packed APIs, while independent known-letter, negative-mask and boundary tests check semantic acceptance.

## Scope and next run

Motor and steering code, GP26 thresholds251/220,1ms sampling, stop-on-rejected/timeout behavior, and30cm/turn calibration hashes matchv5.11. v5.11 UF2 retained for rollback. Artifact and hashes: `build/motion-wireless/build-report-v5_12.json`.

Flashv5.12, refresh the wireless page, and run A then Z once with the existing mounting/prints. Download each log, noting tracking and stopping. Start on white and keep the route fully supported beyond the barcode; no cliff detection exists. If a run fails, retain its first log rather than repeating blindly. No new physical measurements claimed by this build.
