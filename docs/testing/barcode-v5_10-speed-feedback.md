# v5.10: straight-running speed feedback

Built 28 October 2026. Not flashed or physically tested by the agent.

## Evidence and change

The v5.9 A, Z and obstacle logs show clear-line wheel speeds around 2300–2700 counts/s despite a 1200 counts/s target. The old controller had a high starting estimate, a 20% minimum output and insufficient integral correction.

Seventeen clear-line samples per wheel from the three archived logs give median provisional PWM × 1200 / measured-speed estimates of 0.1569 left and 0.1417 right. These are rough seeds, not a fitted motor model: sampled speed can reflect earlier drive power. v5.10 uses 0.157/0.142 seeds, proportional gain 0.000040 and integral gain 0.000200, integral stored in PWM units and bounded to -0.20..0.35. Conditional anti-windup prevents accumulation into output saturation. Straight-running output can range 0..0.50; initial output is 0.20/0.19. Encoder-stall protection remains active.

The target remains 1200 counts/s with existing ±120 counts/s wheel-balance adjustment. Line-edge overrides retain immediate 0.25/0.55 output and freeze speed integration. These corrections may exceed the straight-running target. GP26 thresholds, 1 ms capture, barcode decoder and calibrated 30 cm/turn manoeuvres are unchanged.

The retained TIMING line now includes sample-mean and min/max wheel rates for clear-line control intervals ending at or after 0.5 seconds. It excludes intervals containing a steering override and reports their count separately. These are diagnostic samples, not a measured guarantee of steady-state speed or a time-weighted whole-run average.

## Software verification

Strict Pico W build passed. Host regressions cover measured overspeed authority, upper/lower saturation recovery, two synthetic motor/load scenarios, steering priority, control delays, encoder faults/stall and bounded stops. Barcode acquisition, motion calibration, line checks, motor guard and wireless transport/DHCP regressions passed. Mock browser selection, download, reconnect, reset labelling and mobile layout passed. No tests operated the physical car.

UF2: `build/motion-wireless/robocar_motion_integration_v5_10_wireless.uf2`.
Build metadata: `build/motion-wireless/build-report-v5_10.json`.
The v5.9 UF2 remains unchanged for rollback.

## One combined physical check

1. Flash v5.10 and reconnect to AAI10-Calibrate. Refresh http://192.168.4.1/ and confirm v5.10.
2. Use a continuous flat route of at least 1 m, with a broad box about 60 cm ahead of the sonar. Remove the barcode for this run so barcode decoding cannot end it early.
3. Centre the line between the two central sensors. Select Barcode pass, wait for SELECT, then press GP20; allow the countdown.
4. The intended result is smooth line following and obstacle stopping. GP20 aborts if necessary. Download the log and report smoothness and final clearance.
5. Check actual wheel rates and stop cause before accepting the new gains. Then recheck moving A/Z at the revised speed. Do not equate software simulations with physical validation.
