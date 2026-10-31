# v5.8 controlled driving barcode pass — 28 October 2026

User explicitly requested the driving build after v5.7 captured two complete
hand-swiped A patterns but rejected their width separation. This is a bounded
straight-track diagnostic, not the final autonomous/RTOS course firmware.

Artifact: `build/motion-wireless/robocar_motion_integration_v5_8_wireless.uf2`.
Prior v5.4 moving and v5.7 stationary artifacts remain unchanged. No firmware was
flashed and no actual robot commands/serial monitoring were performed by agent.

## Changes

- Shared GP26 barcode threshold from v5.6/v5.7: black>=251, white<=220,
  previous colour retained between, two-sample confirmation and1ms acquisition.
- Barcode pass replaces open-loop0.40 PWM with encoder velocity feedback:
  nominal1200 counts/s per wheel,100ms updates, existing motion PI/feedforward
  coefficients, barcode-only PWM clamp0.20–0.50 and integral clamp+/-2500.
  Initial PWM0.366 left/0.3477 right. Line edges bias targets+/-200cps; otherwise
  count skew trims targets by at most120cps. Nominal geometry gives89mm/s, but
  actual speed is not yet physically verified; live logs report both wheel cps.
- Existing barcode limits retained:8s,8000 counts on either wheel, encoder
  invalid/wrong-direction/stall, both line sensors black, sampling faults,
  sonar obstacle stop, GP20/browser stop, independent motor-service timeout.
  Added barcode-control late-update stop above250ms. Sonar no-echo remains
  unknown, not evidence of a clear route; sonar does not detect a platform edge.
- Moving freshness checks were already inside interrupt masks. Active check
  now explicitly reads sample timestamp before clock under the mask, logs
  stale age, and retains the same3ms maximum sample-age threshold.
- Retain128 edge timestamps across rejected/incomplete patterns; overflow
  invalidates and stops. Two SCAN EDGES result lines use the existing retained
  result transport. Result still contains widths/rejections/sample gaps.
- Page/firmware/result identifyv5.8. Build script explicitly turns stationary
  modeOFF and creates a new versioned filename. The calibrated motion_control.c
  hash matches the v5.4 build report:30cm/turn targets and controller unchanged.

## Validation

Pico W strict-warning build passed. Host tests cover velocity feedback direction,
steering direction, PWM limits,100ms cadence, late/backward time, invalid encoders,
wrong direction, one-wheel stall, both-black, time/count caps, and a simple
synthetic load step. This is controller logic evidence, not a real motor model.
Existing barcode acquisition, line, guard and motion tests pass, including
measured-level A/Z patterns and threshold boundaries. Wireless core, DHCP and
ordinary/stationary transport tests pass. Mock browser selection/download,
running lockout/reconnect/reset and mobile layout tests pass; robot endpoints
were intercepted, not contacted. UF2 header/family/address/banner checks pass;
old artifacts and calibrated motion source hashes verified. Detailed hashes in
`build/motion-wireless/build-report-v5_8.json`.

## Next physical run

1. User flashesv5.8 and refreshes http://192.168.4.1/ untilV5.8 WIRELESS appears.
2. Use largeA beside the line on a continuous flat route, at least1m clear and
   away from drop-offs. Keep normal mounted height; GP26 starts on white about
   5cm before first bar. Central pair straddles the line. Keep labels/tape clear.
3. Select Barcode pass · A / Z; wait for SELECT BARCODE_PASS. PressGP20 for the
   three-second countdown. Do not hand-slide the sheet during this test.
4. Car stops on decode or limits/faults. GP20 aborts locally; browser stop needs
   connection. Do not reposition until MOTION SETTLED and BARCODE RESULT print.
5. Download oneA run for review before repeatability/Z checks. Desired outcome
   decoded=A valid=1,30 transitions, no faults, and reasonably stable measured
   wheel speed. If rejected, full trace supports offline analysis.

Physical performance remains pending.8s/8000 counts are cut-off limits, not
precise distance or braking guarantees. Do not use the old short raised track
that the robot previously rolled off.
