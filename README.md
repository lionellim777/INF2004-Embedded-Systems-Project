# INF2004 Autonomous Robotic Car

Firmware and engineering records for the INF2004 Embedded Systems Programming group project.

The goal is an autonomous two-wheel robot that follows a line, decodes navigation barcodes, measures humps, avoids obstacles, reacquires the line, and publishes telemetry over Wi-Fi.

## Current status

Latest checkpoint: **v5.34 wireless calibration**, published 31 October 2026.
The [ready-to-flash UF2 and build record](firmware/releases/README.md) are included.
This is a Pico SDK calibration prototype; the final µT-Kernel port and full mission
integration remain pending. Route completion does not establish barcode success:
the latest two v5.34 runs accepted no letters. See the
[raised-height investigation](docs/testing/barcode-v5_34-raised-height-analysis.md).

| Subsystem | Status |
| --- | --- |
| Motor direction and basic PWM | Bench tested |
| Servo safe range | Bench tested: 45-130 degrees; scans use 50-125 degrees |
| Ultrasonic coarse/fine scanning | Bench tested with broad, front-facing targets |
| IMU level calibration and multi-hump state machine | Bench tested |
| Three IR sensors | Current mounting: GP26 dedicated barcode sensor; GP28 directly over route tape; GP27 to its right. Half barcode narrow/wide elements are 3/9 mm; reported GP26 clearance is 2 mm. Optical separation remains unresolved. |
| Line following | Centre-sensor control, encoder feedback, bounded recovery, GP20 start/stop, crossing handling and diagnostics implemented. Operator reports completed home routes; frequent recovery and final line-loss stops remain in logs. |
| Wheel encoders and motion | About 2543 counts/revolution; 30 cm and 90/180-degree calibration incorporated into moving firmware. Recovery non-response has occurred; v5.34 adds bounded per-wheel assistance. |
| Barcode navigation | Dedicated GP26 acquisition at 2 kHz; framed Code 39 A-Z decoder; independent course capture and configurable Y/Z actions. Earlier complete A/Z captures pass replay. Latest moving half-A/Z reads fail; no claim of reliable half-size or turning acceptance. |
| Obstacle bypass and line recovery | Sonar stopping and bounded line recovery are implemented in calibration mode. Integrated autonomous obstacle bypass remains pending. |
| Wi-Fi/MQTT telemetry | Local Wi-Fi access point, browser controls and downloadable logs implemented. This calibration page is not the final MQTT subsystem. |

Detailed evidence and limitations are recorded in [docs/development-status.md](docs/development-status.md).

## Repository layout

```text
docs/                         Project requirements, design and wiring records
firmware/app/robot_firmware/  Moving firmware, acquisition, control and local web UI
firmware/tests/               Hardware diagnostics and host regression tests
firmware/releases/            Published calibration UF2 and verification record
tools/                        Build, replay, test and data-budget helpers
output/pdf/                   Printable barcode calibration sheet
```

Standalone hardware test folders contain Pico SDK CMake projects. Host tests use
the scripts in `tools/`. Build caches, toolchain downloads and intermediate files
are excluded from Git; the selected release UF2 is included explicitly.

## Toolchain

- Raspberry Pi Pico W (RP2040)
- Raspberry Pi Pico SDK (v5.34 verified with 2.1.1)
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
