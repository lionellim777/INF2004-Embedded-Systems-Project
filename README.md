# INF2004 Autonomous Robotic Car

Firmware and engineering records for the INF2004 Embedded Systems Programming group project.

The goal is an autonomous two-wheel robot that follows a line, decodes navigation barcodes, measures humps, avoids obstacles, reacquires the line, and publishes telemetry over Wi-Fi.

## Current status

Latest checkpoint: **v5.63 barcode baseline**,7 November2026.
Two physical home-track runs each accepted full-A, half-A and half-Z on the right
bend: **AAZ / AAZ,6/6 target reads**, without sampling-integrity faults. The
measured3:1 print-geometry reader and bounded recovery are the working baseline.
See [barcode acceptance](docs/testing/barcode-v5_63-acceptance.md),
[progress](docs/development-status.md), and
[remaining project work](docs/project-next-steps.md).
The [UF2s and verification records](firmware/releases/README.md) contain the
validated image and historical checkpoints. This remains a Pico SDK calibration
prototype: final mission/µT-Kernel/MQTT integration is pending, and barcode reading
is currently disabled in obstacle mode. Acceptance applies to the tested layout,
not universal reliability or the whole final mission.

| Subsystem | Status |
| --- | --- |
| Motor direction and basic PWM | Bench tested |
| Servo safe range | Bench tested: 45-130 degrees; scans use 50-125 degrees |
| Ultrasonic coarse/fine scanning | Bench tested with broad, front-facing targets |
| IMU level calibration and multi-hump state machine | Bench tested |
| Three IR sensors | GP26 dedicated barcode sensor (+80mm forward/+85mm left of powered axle); GP28 centred on18mm tape; GP27 is20mm right of GP28. Measured half3/9mm and full6/18mm bars/gaps. |
| Line following | Centre/right control, encoder feedback, bounded expanding recovery and GP20 controls. Two AAZ home routes including the right bend completed; whole-course/junction acceptance remains. |
| Wheel encoders and motion | About 2543 counts/revolution; 30 cm and 90/180-degree calibration incorporated into moving firmware. School runs recorded right-wheel non-progress during loaded turns. v5.53 increases bounded turn effort and preserves per-wheel fault evidence. Subsequent straight-strip bypass completed; broader repeatability remains unverified. |
| Barcode decoding / navigation | Decoding accepted on the current home setup: AAZ twice, including half-Z during turning. A-Z host tests and configurable Y/Z route actions exist; final command mapping and physical navigation manoeuvres remain separate. |
| Obstacle bypass and line recovery | Bounded right-side bypass and perpendicular tape alignment implemented. Earlier home runs resumed following. v5.53 straight-strip bypass completed, while the bend rejoin lost tape. v5.56 restores right-first rejoin and adds scan-planned departure with left-side clearance checks; physical retests pending. |
| Wi-Fi/MQTT telemetry | Main has a local calibration AP/page/logs. `wifi-buddy-1` has standalone µT-Kernel/MQTT work and validation records; real-producer/final-robot integration remains. |

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
