# Development and test status

Last updated: 22 September 2026.

## Verified on the assembled robot

### Motor bring-up

- Both motors have been driven in the expected forward direction.
- The right motor polarity was corrected during bring-up.
- GP20 start/stop behaviour was demonstrated in the integration prototype.
- Encoder feedback has not yet been connected or tested.

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

- Three analogue IR connections have been assigned to GP26, GP27 and GP28.
- Earlier readings demonstrated strong light/dark contrast on two sensors.
- Three-sensor calibration and physical track testing are pending.
- The current integration prototype uses only two sensors and must not be treated as final line-following firmware.

## Standalone firmware tests

| Program | Purpose |
| --- | --- |
| `robot_bringup` | Combined hardware connectivity and manual actuator checks |
| `servo_range_test` | Establish safe servo travel |
| `ultrasonic_scan_test` | Coarse/fine obstacle scans and profile diagnostics |
| `imu_hump_test` | Automatic level calibration and multi-hump detection |
| `ir_calibration_test` | Capture light/dark values for three IR sensors |

## Next development priorities

1. Identify and wire both wheel encoders.
2. Count quadrature pulses and determine counts per wheel revolution.
3. Calibrate wheel circumference, distance and turn accuracy.
4. Complete three-sensor IR calibration on the actual track.
5. Refactor proven drivers into reusable modules.
6. Add Wi-Fi/MQTT telemetry and the final mission state machine.

