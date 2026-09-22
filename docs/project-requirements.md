# Project requirements

This summary is derived from the INF2004 Autonomous Robotic Car project briefing. The official module documents remain authoritative.

## Mission requirements

The completed robot must:

1. Follow a line autonomously.
2. Detect and decode navigation barcodes.
3. Execute left, right, straight and U-turn commands.
4. Detect humps.
5. measure and report the highest hump peak encountered.
6. Detect obstacles on or near the line.
7. Profile obstacle position and shape using ultrasonic sensing.
8. Navigate around obstacles without collision.
9. Reacquire the original line after bypassing an obstacle.
10. Publish robot status and telemetry through Wi-Fi.

## Required technical areas

| Area | Expected capability |
| --- | --- |
| Communications | Wi-Fi connection, MQTT topics, telemetry, commands, heartbeat and recovery |
| Motion | Motor driver, wheel encoders, speed/distance estimation, PID control and repeatable turns |
| Line/navigation | Three-sensor calibration, line position, junctions, barcode decoding and commands |
| Terrain | IMU calibration, tilt, hump/peak detection, impacts and motion events |
| Obstacles | Coarse/fine scanning, profiling, bypass decisions and line recovery |

## Assessment focus

- Successful mission completion
- Navigation accuracy
- Perception reliability
- Robust recovery and stable operation
- Software quality and resource efficiency
- Clean subsystem integration
- Clear test evidence and demonstration

