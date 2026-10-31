# v5.11 — failed-capture stops and speed-feedback steering

Built28 October2026. Physical validation pending; no robot controls operated by agent.

## Changes

Moving barcode capture latches the first rejected frame or750ms element timeout. Foreground motor handling converts these to BARCODE_REJECTED / BARCODE_CAPTURE_TIMEOUT with zero motor drive; it does not keep driving while starting another frame. Sample faults retain highest priority; existing drive stop conditions retain priority over capture outcomes. Failed frame widths remain available after later samples; stationary capture retains its existing retry behavior. Handling occurs on the next foreground service after detection, not in the sampling IRQ. Existing150ms motor service guard remains.

Barcode steering no longer overrides feedback with0.25/0.55PWM. It requests900cps inside /1200cps outside, with target slew1500cps/s (0.2s for300cps). Feedback remains active during turns; no requested wheel target exceeds1200. This does not guarantee physical speed cannot overshoot. Straight balance slows the leading wheel by up to120cps. Normal line-check mode and calibrated30cm/turn controller are unchanged. Weaker turning at this regulated speed requires actual line-tracking validation.

Added raw sample bins: <=220white,221..250hysteresis band,>=251black; single-sample white dips rejected by debounce; longest classified-black duration. TIMING retains these counters. Live logs include both current wheel-speed targets. Thresholds remain251/220, sampling1ms; decoder acceptance rules are unchanged. These aggregates help distinguish absent contrast from debounce but cannot reconstruct every raw white gap or prove mounting geometry.

## Evidence and checks

Replay of recorded v5.10 A1/A2/Z1/Z2/Z3 edges preserves A/Z successes and gives explicit A2 rejection / Z1 timeout stops. A2 retained29 durations and the133ms guard element. Timeout boundary and stuck-level tests passed. Host steering simulations exercise both directions, ramp limits, load steps and recovery; no claim of real robot behavior. ASan/UBSan regressions, strict Pico W build, wireless transport tests,18 stationary-main scenarios and mocked browser checks passed. Build report contains artifact/source hashes.

## Next user run

Flash `build/motion-wireless/robocar_motion_integration_v5_11_wireless.uf2`, reconnect to AAI10-Calibrate and refresh the page to v5.11. Use the existing print and sensor mounting. Start on white about5cm before the barcode. Select Barcode pass, wait for SELECT, press GP20. Run A then Z and save each log after settling. If a scan fails, save its first result before further retries.

The route must remain continuously supported beyond the barcode. There is still no cliff detector. A scan that never starts can still run to the existing8s/8000count bound; stop-on-capture-failure is not drop-off protection. GP20 is the local abort. No obstacle bypass or course navigation added.
