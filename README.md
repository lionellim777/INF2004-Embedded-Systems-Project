# INF2004 Autonomous Robotic Car

Firmware and engineering records for the INF2004 Embedded Systems Programming group project.

The goal is an autonomous two-wheel robot that follows a line, decodes navigation barcodes, measures humps, avoids obstacles, reacquires the line, and publishes telemetry over Wi-Fi.

## Current status

| Subsystem | Status |
| --- | --- |
| Motor direction and basic PWM | Bench tested |
| Servo safe range | Bench tested: 45-130 degrees; scans use 50-125 degrees |
| Ultrasonic coarse/fine scanning | Bench tested with broad, front-facing targets |
| IMU level calibration and multi-hump state machine | Bench tested |
| Three IR sensors | Light/dark calibration recorded for all three; straight-line bench testing underway |
| Line following | Three-sensor control, local lost-line recovery, GP20 start/stop, and diagnostics implemented; full-course testing pending |
| Wheel encoders and PID motion | Encoder counting code compiles; wiring and physical test pending, PID not implemented |
| Barcode decoding | Not started |
| Obstacle bypass and line recovery | Not started |
| Wi-Fi/MQTT telemetry | Not started |

Detailed evidence and limitations are recorded in [docs/development-status.md](docs/development-status.md).

## Repository layout

```text
docs/                         Project requirements, design and wiring records
firmware/app/robot_firmware/  Current three-sensor integration firmware and modules
firmware/tests/               Standalone hardware test programs
```

Each test folder is an independent Raspberry Pi Pico SDK CMake project and produces its own UF2 file. Build products are intentionally excluded from Git.

## Toolchain

- Raspberry Pi Pico W (RP2040)
- Raspberry Pi Pico SDK 2.3.0 or compatible
- CMake and Ninja
- ARM GNU toolchain
- USB serial monitor

Set `PICO_SDK_PATH` to the Pico SDK directory before configuring a project.

Example using the IMU test:

```powershell
cmake -S firmware/app/robot_firmware -B firmware/app/robot_firmware/build -G Ninja -DPICO_BOARD=pico_w
cmake --build firmware/app/robot_firmware/build
```

Flash the generated `.uf2` file by holding `BOOTSEL` while connecting the Pico W, then copy the UF2 to the `RPI-RP2` drive.

## Safety

- The robot must start with both motors stopped.
- Raise the driven wheels before running an unverified motor test.
- Keep motor power wiring away from exposed logic pins.
- Do not power motors directly from Pico GPIO pins.
- Treat an ultrasonic `no echo` result as unknown, not guaranteed clear space.
- Recheck the wiring table before changing Grove ports or GPIO assignments.

## Documentation

- [Project requirements](docs/project-requirements.md)
- [Architecture](docs/architecture.md)
- [Hardware wiring](docs/hardware-wiring.md)
- [Development and test status](docs/development-status.md)
- [Contribution workflow](CONTRIBUTING.md)
