# Wireless calibration checkpoint v5.47

| Build | Firmware | Evidence |
| --- | --- | --- |
| v5.47 — latest | [Download UF2](robocar_motion_integration_v5_47_wireless.uf2) | Conditional faster cruise and transit; software checks passed, physical speed and straight-strip bypass tests pending. [Verification record](build-report-v5_47.json) |
| v5.46 — baseline | [Download UF2](robocar_motion_integration_v5_46_wireless.uf2) | One observed bend-obstacle bypass completed and resumed following. Full straight-strip return not yet demonstrated. [Verification record](build-report-v5_46.json) |
| v5.34 — historical | [Download UF2](robocar_motion_integration_v5_34_wireless.uf2) | Earlier barcode calibration checkpoint. [Verification record](build-report-v5_34.json) |

Target: Raspberry Pi Pico W / RP2040 on Robo Pico; GP28 centred over route tape,
GP27 to its right, GP26 dedicated to barcodes.

Hold BOOTSEL while connecting USB, then copy the chosen UF2 to RPI-RP2.
The robot starts stopped. Join the documented AAI10-Calibrate access point,
open http://192.168.4.1/, select **Follow track + avoid obstacle**, prepare the run,
then use GP20 to start/stop. See [wireless instructions](../../docs/wireless-calibration.md)
and [obstacle test procedure](../../docs/testing/obstacle-v5_35-bypass.md).

v5.47 increases speed only with stable tape and fresh clear forward readings;
turns and tape reacquisition retain the cautious profile. Barcodes are disabled
in obstacle mode. Half-size barcode reliability, the final mission and the
µT-Kernel port remain incomplete. The successful bend test does not establish
arbitrary-bend capability or full straight-strip obstacle clearance.

## Build from source

The local convenience script `tools/build_wireless_calibration.sh` expects Pico SDK 2.1.1, ARM GNU 14.2.rel1 for macOS ARM64, CMake/Ninja and picotool under the existing ignored `build/` dependency directories. Those downloads are not included. On another machine, install the Pico SDK/toolchain and use its normal CMake setup, with:

```sh
python3 tools/embed_calibration_page.py
cmake -S firmware/app/robot_firmware -B build/release -G Ninja -DPICO_BOARD=pico_w -DROBOCAR_CENTER_LINE=ON -DROBOCAR_WIRELESS_CALIBRATION=ON -DROBOCAR_STATIONARY_BARCODE=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

Set PICO_SDK_PATH and the toolchain path for your installation. `build/release/robot_firmware.uf2` is the resulting image. The repository includes generated web headers; regenerate them after HTML edits.

## Validation

Latest checkpoint host checks: `tools/test_obstacle_bypass.py`, `tools/test_barcode_calibration.py`, `tools/test_course_main.py --center`, `tools/test_course_main.py`, `tools/test_wireless_calibration.py`, `tools/test_wireless_dhcp_lwip.py`. These scripts currently contain local macOS SDK assumptions; adjust SDKROOT for another host. They test software and simulated inputs, not physical track acceptance.
