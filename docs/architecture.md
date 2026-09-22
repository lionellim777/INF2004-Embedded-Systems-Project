# Software architecture

The final application should coordinate independent modules through a small vehicle state machine.

```text
Sensors
  IR line sensors   Wheel encoders   IMU   Ultrasonic scanner
        \                 |           |           /
         \                |           |          /
          Line/navigation |      Terrain     Obstacle profile
                    \     |          |       /
                     Vehicle controller
                       /           \
               Motion control    Wi-Fi/MQTT telemetry
                       |
                    Motors
```

## Proposed application states

```text
BOOT
  -> CALIBRATING
  -> FOLLOWING_LINE
  -> EXECUTING_BARCODE_COMMAND
  -> STOPPING_FOR_OBSTACLE
  -> SCANNING_OBSTACLE
  -> BYPASSING_OBSTACLE
  -> REACQUIRING_LINE
  -> FOLLOWING_LINE

Any state -> FAULT/SAFE_STOP
```

## Module boundaries

| Module | Owns | Example interface |
| --- | --- | --- |
| `motor` | PWM direction and emergency stop | `motor_set()`, `motors_stop()` |
| `encoder` | Pulse counting and speed/distance | `encoder_get_count()` |
| `motion` | PID and distance/angle commands | `move_forward()`, `turn_left()` |
| `line_sensor` | Three ADC readings and calibration | `line_get_position()` |
| `barcode` | Pattern decoding | `barcode_poll()` |
| `imu` | Level calibration and motion events | `imu_poll()`, `imu_get_hump_count()` |
| `ultrasonic` | Distance samples | `ultrasonic_read_cm()` |
| `scanner` | Servo scan and obstacle profile | `scanner_run()` |
| `telemetry` | MQTT publication and commands | `telemetry_publish()` |
| `controller` | Mission state and ownership | `controller_step()` |

Modules should not drive each other's hardware directly. The controller selects behaviour using module results and commands.

## Failure policy

- Sensor timeout: report invalid data and retain a safe state.
- Ultrasonic no echo: treat as unknown rather than automatically clear.
- Lost line: stop or execute a bounded search procedure.
- Wi-Fi loss: continue safe local control and retry communication.
- Unexpected state or watchdog reset: motors off.

