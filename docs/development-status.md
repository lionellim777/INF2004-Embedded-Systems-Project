# Development and test status

Last updated: 23 September 2026.

## Verified on the assembled robot

### Motor bring-up

- Both motors have been driven in the expected forward direction.
- The right motor polarity was corrected during bring-up.
- GP20 start/stop behaviour was demonstrated in the integration prototype.
- Encoder counting code has been added but encoder feedback has not yet been connected or tested.

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

## Next development priorities

1. Obtain eight male jumper wires and connect the four encoder leads from each motor to Grove 2 and Grove 4 through the Grove breakout cables, after confirming 3.3 V compatibility.
2. With wheels raised and motors stopped, turn each wheel by hand and verify that only its own `EncL` or `EncR` count changes.
3. Determine encoder counts per wheel revolution and calibrate measured distance and turns.
4. Test the three-sensor line follower and recovery on the actual track; tune speed and thresholds.
5. Implement encoder-based motion control, barcode navigation, obstacle bypass, and Wi-Fi/MQTT telemetry.
