# Development and test status

Last updated: 24 September 2026.

## Verified on the assembled robot

### Motor bring-up

- Both motors have been driven in the expected forward direction.
- The right motor polarity was corrected and reverified during the raised-wheel
  encoder motor test.
- GP20 start/stop behaviour was demonstrated in the integration prototype.
- Both encoder A/B channels have been hand-rotation tested with no invalid
  transitions. Forward count signs are left negative and right positive.
- A repeated ten-revolution hand test measured approximately 2539 counts per
  revolution on the left and 2547 on the right; 2543 is the initial shared
  calibration value.
- At equal 35% PWM with both wheels raised, the left wheel measured about 3229
  counts/s and the right about 3393 counts/s. The right wheel was 4.9% faster;
  both directions were correct and both invalid-transition counts remained zero.
- The standalone PI controller targeted 3000 counts/s and measured 3000 counts/s
  left and 3017 counts/s right over eight seconds. It reduced the mismatch to
  0.5% using approximately 32.5% left and 30.8% right PWM, with zero invalid
  transitions. Integration into the main motion module remains pending.
- The Batch 5 floor test completed 30 cm straight, right 90 degrees, left 90
  degrees, and right 180 degrees in one run. The operator reported that the
  motions looked good. Final straight counts were 4255 left and 4217 right;
  peak wheel-count skew during travel was 56 counts. All four stages reported
  zero invalid encoder transitions. This is a visual floor check, not a
  measured-angle or full-course acceptance test.

### Ultrasonic sensor and servo

- HC-SR04+ distance readings respond to suitable front-facing targets.
- Coarse and fine servo scans complete successfully.
- The scan range is restricted to 50-125 degrees to respect the mechanical installation.
- Small or edge-on objects may reflect sound away from the receiver and return `no echo`.
- The calculated scan footprint is not yet a calibrated physical object width.

### GY-511 accelerometer

- I2C communication and live acceleration readings are working.
- Automatic two-second level calibration is working.
- Calibration is rejected when the car moves excessively during sampling.
- Climbing, descending and level orientations are distinguishable.
- A state machine detects normal and back-to-back hump sequences.
- Incomplete or timed-out sequences are cancelled.
- Hump count and highest climb angle are reported.

Bench demonstration result:

| Hump | Peak climb | Peak descent | Duration |
| --- | ---: | ---: | ---: |
| 1 | +24.3 degrees | -5.2 degrees | 3110 ms |
| 2 | +36.6 degrees | -6.4 degrees | 8292 ms |

The second hump was correctly reported as the highest climb angle. Actual hump-height estimation and real-track tuning remain pending.

### IR sensors and line following

- All three analogue IR sensors have been measured on the light styrofoam and dark line.
- Physical left/centre/right order is Grove 5/7/6, using GP26/28/27 respectively.
- Three-sensor following, GP20 start/stop, short-gap crossing, and bounded lost-line recovery are implemented.
- Serial diagnostics report the run state, line state, sensor readings, drive commands, range, and terrain status.
- Straight-line bench testing has begun; full-course line and junction performance is not yet verified.

### Integrated safety behaviour

- The robot starts with both motors stopped and waits for GP20.
- Two consecutive valid ultrasonic readings below 30 cm stop and disarm the motors.
- A missing ultrasonic echo is treated as unknown, not as a confirmed clear path.
- The sonar stop is not an obstacle-bypass implementation.

## Standalone firmware tests

| Program | Purpose |
| --- | --- |
| `robot_bringup` | Combined hardware connectivity and manual actuator checks |
| `servo_range_test` | Establish safe servo travel |
| `ultrasonic_scan_test` | Coarse/fine obstacle scans and profile diagnostics |
| `imu_hump_test` | Automatic level calibration and multi-hump detection |
| `ir_calibration_test` | Capture light/dark values for three IR sensors |
| `encoder_wiring_test` | Motors held off; report A/B edges and quadrature counts while each raised wheel is turned by hand. Hardware verified; no invalid transitions observed. |
| `encoder_motor_test` | With wheels raised, run left, right, and both motors at 35%, followed by a 3000 counts/s PI-controlled stage. All stages hardware verified. |
| `motion_control_test` | Defaults to one run of straight 30 cm, right 90 degrees, left 90 degrees, and right 180 degrees; individual motions remain selectable with GP21. Each completed stage is saved to flash and the full report is replayed after reconnection. Batch 5 completed all four stages; the operator reported good straight and turn behaviour, and every stage had zero invalid encoder transitions. This standalone calibration is ready for main-firmware integration. |

## Next development priorities

1. Integrate the verified speed and turn control into the main robot firmware.
2. Test the three-sensor line follower and recovery on the actual track; tune speed and thresholds.
3. Implement barcode navigation, obstacle bypass, and Wi-Fi/MQTT telemetry.
