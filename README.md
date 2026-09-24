# INF2004 autonomous car — Buddy 1

This is a work-in-progress Buddy 1 Pico W communication subsystem. It uses
micro T-Kernel 3.0, Raspberry Pi's Pico C SDK, CYW43 WiFi, lwIP MQTT, and
Mosquitto on a laptop. The robot's navigation code remains independent of
network availability.

The project brief calls for a course template. This repository began with only
this README; no other template files were present. Compare any later course
starter with this layout before integrating the team's other modules.

## Build the Pico W firmware

Install CMake, Ninja, Python, and Arm GNU Toolchain. The verified source
revisions for this project are:

- Pico SDK tag `2.2.0`, including its submodules.
- TRON Forum `mtk3_bsp` branch `pico_rp2040`, commit
  `15ed232c08f2d89e54e514db79de150e224307e3`.

Clone both outside this repository or into ignored `vendor/` directories.
Check out the exact BSP commit. Copy `config/comm_secrets.example.h` to
`config/comm_secrets.h` and replace all placeholders with private network
values. Then build:

```powershell
$env:PICO_SDK_PATH = 'C:/path/to/pico-sdk'
$env:MTK_BSP_PATH = 'C:/path/to/mtk3_bsp'
cmake -S . -B build/pico -G Ninja -DPICO_BOARD=pico_w
cmake --build build/pico
```

Flash `build/pico/buddy1_pico_w.uf2` with BOOTSEL or a compatible SWD tool.
The example configuration cannot connect until its placeholders are replaced.
The firmware build and USB behavior still need verification on Pico W hardware.

The Pico SDK owns reset, flash boot stage, clocks, C runtime, and the vector
table. The port registers the micro T-Kernel PendSV and SysTick handlers,
provides a fixed 96 KiB kernel pool, and checks the expected 125 MHz clock.
Core 1 stays unused. The original BSP's GPIO25 LED sample and old startup
files are excluded because Pico W uses those pins for its wireless device.

## Host checks

```powershell
cmake -S . -B build/host -G Ninja -DBUDDY1_HOST_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
python tools/check_barr.py
```

Host checks cover command parsing and bounded telemetry serialization. The
firmware build, flashing, network recovery, timing, and endurance require
additional Pico W and robot hardware checks.

Read [the MQTT contract](docs/mqtt.md), [the integration guide](docs/integration.md),
and [the BARR-C review checklist](docs/barr-c-review.md) before connecting
other buddies' producers.
