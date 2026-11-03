# Wireless calibration checkpoint v5.55

| Build | Firmware | Evidence |
| --- | --- | --- |
| v5.55 — latest | [Download UF2](robocar_motion_integration_v5_55_wireless.uf2) | Spatial ink check, approach-heading alignment and bounded endpoint assistance; physical retest pending. [Verification record](build-report-v5_55.json) |
| v5.54 — previous | [Download UF2](robocar_motion_integration_v5_54_wireless.uf2) | Recovery-direction handoff and slower, wider bounded rejoin verification; software checks passed, physical retest pending. [Verification record](build-report-v5_54.json) |
| v5.53 — previous | [Download UF2](robocar_motion_integration_v5_53_wireless.uf2) | 25 cm initial detour, completed-no-return tape rejoin, loaded-turn effort and retained failure reports. Straight-strip bypass observed complete; bend rejoin subsequently lost tape. [Verification record](build-report-v5_53.json) |
| v5.47 — historical | [Download UF2](robocar_motion_integration_v5_47_wireless.uf2) | Conditional faster cruise and transit; software checks passed, physical speed and straight-strip bypass tests pending. [Verification record](build-report-v5_47.json) |
| v5.46 — baseline | [Download UF2](robocar_motion_integration_v5_46_wireless.uf2) | One observed bend-obstacle bypass completed and resumed following. Full straight-strip return not yet demonstrated. [Verification record](build-report-v5_46.json) |
| v5.34 — historical | [Download UF2](robocar_motion_integration_v5_34_wireless.uf2) | Earlier barcode calibration checkpoint. [Verification record](build-report-v5_34.json) |

Target: Raspberry Pi Pico W / RP2040 on Robo Pico; GP28 centred over route tape,
GP27 to its right, GP26 dedicated to barcodes.

Hold BOOTSEL while connecting USB, then copy the chosen UF2 to RPI-RP2.
The robot starts stopped. Join the documented AAI10-Calibrate access point,
open http://192.168.4.1/, select **Follow track + avoid obstacle**, prepare the run,
then use GP20 to start/stop. See [wireless instructions](../../docs/wireless-calibration.md)
and [obstacle test procedure](../../docs/testing/obstacle-v5_35-bypass.md).

v5.55 retains the 25 cm initial lateral target, 20 cm obstacle detection and
bounded turn assistance. It checks 60 mm across candidate ink before accepting
a crossing, chooses alignment towards the approach heading and can assist a
loaded wheel near a slow forward endpoint up to 32% drive. Stall, sonar and
travel limits remain. Recovery-direction memory and slower verification from
v5.54 are retained. See the test procedure for acceptance and rejection markers.

The latest build is not physically validated. Half-size barcode reliability,
the final mission and the µT-Kernel port remain incomplete. Barcodes are disabled
in obstacle mode. Historical successful home tests do not establish real-course
acceptance. See [progress](../../docs/development-status.md) for observed failures
and the distinction between simulations and physical tests.

## Build from source

The local convenience script `tools/build_wireless_calibration.sh` expects Pico SDK 2.1.1, ARM GNU 14.2.rel1 for macOS ARM64, CMake/Ninja and picotool under the existing ignored `build/` dependency directories. Those downloads are not included. On another machine, install the Pico SDK/toolchain and use its normal CMake setup, with:

```sh
python3 tools/embed_calibration_page.py
cmake -S firmware/app/robot_firmware -B build/release -G Ninja -DPICO_BOARD=pico_w -DROBOCAR_CENTER_LINE=ON -DROBOCAR_WIRELESS_CALIBRATION=ON -DROBOCAR_STATIONARY_BARCODE=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

Set PICO_SDK_PATH and the toolchain path for your installation. `build/release/robot_firmware.uf2` is the resulting image. The repository includes generated web headers; regenerate them after HTML edits.

## Validation

Available host checks (the verification record lists those run for each release): `tools/test_obstacle_bypass.py`, `tools/test_center_bend.py`, `tools/test_barcode_calibration.py`, `tools/test_course_main.py --center`, `tools/test_course_main.py`, `tools/test_wireless_calibration.py`, `tools/test_wireless_dhcp_lwip.py`. These scripts currently contain local macOS SDK assumptions; adjust SDKROOT for another host. They test software and simulated inputs, not physical track acceptance.
