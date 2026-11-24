# Final firmware: RTOS mission image 0.6-dev3

This is the firmware submitted for the project. Every other version name in the
repository (v5.x, v0.5.1, `-lean`, dev1, dev2) is earlier work or a variant
and is kept for history only.

## Identity

| Item | Value |
| --- | --- |
| Target | `firmware/app/mission_rtos` (µT-Kernel on Raspberry Pi Pico W) |
| Version string | `0.6-dev3`, printed at boot and sent in the MQTT status |
| Source | commit `2317ab4` on `main`; later commits change only documentation, build defaults and the build script |
| Size (Release) | 491,420 B text, 208,736 B bss, 0 B data |
| Tested image | SHA-256 `1f761c2e329d408e6aff7501645f4b30f3f072f35cbf0b44356b13cc10808a2d` |

The tested UF2 is not committed. A build embeds the Wi-Fi settings from the
ignored `network_settings.local.h`, so a published UF2 would carry them. A
rebuild from this source with the defaults below is byte-identical to the tested
image except for the build-date string the Pico SDK stores in the binary.

## What it does

- Line following, junction handling and curve following (`course_control.c`).
- Barcode decoding with configurable letter actions (`barcode_actions.c`).
- Obstacle detection by sonar scan, left bypass and line rejoin, including the
  obstacle width (`obstacle_bypass.c`, `obstacle_io.c`).
- Hump and tilt terrain telemetry from the IMU (`terrain_telemetry.c`).
- MQTT telemetry over Wi-Fi in a binary format, with a shared 16-bit fault mask
  (motor stall, encoder, IMU, no echo, Wi-Fi loss, MQTT loss).
- Minimum-bit state packing and BARR-C layout, described in
  [minimum-bit-representation.md](minimum-bit-representation.md).

## Build

The CMake defaults are the tested configuration:

| Option | Default | Meaning |
| --- | --- | --- |
| `RTOS_COMPACT_ADC_STATE` | ON | minimum-bit ADC filter state |
| `RTOS_COMPACT_CORE_STATE` | ON | minimum-bit baseline and capture state |
| `RTOS_COMPACT_CONTROL_STATE` | ON | minimum-bit controller, obstacle and flag state |
| `RTOS_OBSTACLE` | ON | obstacle bypass and rejoin |
| `RTOS_TERRAIN` | ON | terrain telemetry |
| `RTOS_CURVE_FOLLOW` | ON | curve steering |
| `RTOS_LAUNCH_CAP` | ON | first-launch breakaway power |
| `RTOS_FIRST_FAILURE_CACHE` | ON | keep the first failed barcode waveform |

Set `PICO_SDK_PATH` (Pico SDK 2.3.1) and `MTK_BSP_PATH` (the pinned µT-Kernel
BSP, checked by CMake), then:

```sh
cmake -S firmware/app/mission_rtos -B build/final -G Ninja \
    -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Release
cmake --build build/final
```

The output is `build/final/robocar_rtos_motion.uf2`. `tools/build_rtos_motion.sh`
runs the same configuration. To use your own Wi-Fi, copy
`network_settings.example.h` to `network_settings.local.h` in
`firmware/app/mission_rtos` and edit it; that file is ignored by Git.

Switching `RTOS_COMPACT_*` OFF gives the unpacked reference image. It runs the
same logic and costs about 400 B more RAM. The layout-equivalence tests below
show the two behave identically.

## Verification

- Host layout-equivalence tests give identical hashes with packing on and off:
  obstacle `afae44f1734c9795`, observer `178881810266fad0`, course
  `84a8d52837b9152b`.
- `obstacle_align_sweep_test` covers the rejoin line-up and fails on the code
  before the fix.
- `tools/check_barr.py` reports 0 findings on 88 project-owned files.
- The image was run on the car and the project owner reports the remaining
  field issues are resolved.

## Older documents

`development-status.md`, `project-next-steps.md`, `rtos-module-handoff.md` and
`HANDOFF-2026-11-10.md` describe the calibration prototype and the v0.5.1
baseline as they stood on 7 to 10 November 2026. They are kept as a record and
do not describe this image.
