# Wireless calibration checkpoint v5.34

- Firmware: [robocar_motion_integration_v5_34_wireless.uf2](robocar_motion_integration_v5_34_wireless.uf2)
- Target: Raspberry Pi Pico W / RP2040 on Robo Pico; centred GP28 sensor arrangement.
- SHA-256: `3f12def79c2961d4438417d3e90c1e6a0454f6cfced7d9115104afce3abb3568`
- [Verification record](build-report-v5_34.json)

Hold BOOTSEL while connecting USB, then copy the UF2 to RPI-RP2. The robot starts stopped. Join the documented AAI10-Calibrate access point, open http://192.168.4.1/, prepare a test, and use GP20 to start/stop. See [wireless instructions](../../docs/wireless-calibration.md). This is the existing v5.34 build, not a new barcode fix.

Current limitation: the latest moving half-A/Z tests produced zero accepted letters, despite reliable sampling. The [optical investigation](../../docs/testing/barcode-v5_34-raised-height-analysis.md) distinguishes this from decoder replay success. Do not describe the final mission, obstacle bypass, µT-Kernel port or half-barcode acceptance as complete.

## Build from source

The local convenience script `tools/build_wireless_calibration.sh` expects Pico SDK 2.1.1, ARM GNU 14.2.rel1 for macOS ARM64, CMake/Ninja and picotool under the existing ignored `build/` dependency directories. Those downloads are not included. On another machine, install the Pico SDK/toolchain and use its normal CMake setup, with:

```sh
python3 tools/embed_calibration_page.py
cmake -S firmware/app/robot_firmware -B build/release -G Ninja -DPICO_BOARD=pico_w -DROBOCAR_CENTER_LINE=ON -DROBOCAR_WIRELESS_CALIBRATION=ON -DROBOCAR_STATIONARY_BARCODE=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

Set PICO_SDK_PATH and the toolchain path for your installation. `build/release/robot_firmware.uf2` is the resulting image. The repository includes generated web headers; regenerate them after HTML edits.

## Validation

Host checks: `tools/test_barcode_calibration.py`, `tools/test_course_main.py --center`, `tools/test_course_main.py`, `tools/test_wireless_calibration.py`, `tools/test_wireless_dhcp_lwip.py`. These scripts currently contain local macOS SDK assumptions; adjust SDKROOT for another host. They test software and simulated inputs, not physical track acceptance.
