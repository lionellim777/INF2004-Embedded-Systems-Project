# Wireless calibration checkpoint v5.63

| Build | Firmware | Evidence |
| --- | --- | --- |
| v5.63 — latest | [Download UF2](robocar_motion_integration_v5_63_wireless.uf2) | Measured3/9mm and6/18mm centre-spacing model, lateral recross handling and wider bounded search. Two physical home-track runs accepted AAZ (6/6 target reads), including bend half-Z; final integration remains. [Verification record](build-report-v5_63.json) |
| v5.62 — previous | [Download UF2](robocar_motion_integration_v5_62_wireless.uf2) | Lost-confirmation search retry, measured right-preview rearming, bounded two-profile width consensus. Saved bend-Z batch replay passes; physical retest pending. [Verification record](build-report-v5_62.json) |
| v5.61 — previous | [Download UF2](robocar_motion_integration_v5_61_wireless.uf2) | GP26 pose/distance widths, pause/recross handling and independent scan-attempt diagnostics; v5.60 steering retained. Physical AAZ retest pending. [Verification record](build-report-v5_61.json) |
| v5.60 — previous | [Download UF2](robocar_motion_integration_v5_60_wireless.uf2) | Earlier gentle drift correction; hard right arcs require timed live GP27 evidence. Three reported bend traversals, accepted sequences empty/A/empty. [Verification record](build-report-v5_60.json) |
| v5.59 — previous | [Download UF2](robocar_motion_integration_v5_59_wireless.uf2) | Two observed straight-section failures: recovery history triggered unsupported right arcs. Superseded by v5.60 confidence gate. [Verification record](build-report-v5_59.json) |
| v5.58 — previous | [Download UF2](robocar_motion_integration_v5_58_wireless.uf2) | Physical straight half-Z accepted once; five A/A/bend runs read AA in all five, no Z, rolling bend never activated. [Verification record](build-report-v5_58.json) |
| v5.57 — previous | [Download UF2](robocar_motion_integration_v5_57_wireless.uf2) | Initial waveform reader; observed half-Z rejection. Superseded diagnostic retention and whole-capture framing. [Verification record](build-report-v5_57.json) |
| v5.56 — previous | [Download UF2](robocar_motion_integration_v5_56_wireless.uf2) | Right-first rejoin, scan-planned departure, side checks and faster bounded following. Physical validation pending. [Verification record](build-report-v5_56.json) |
| v5.55 — previous | [Download UF2](robocar_motion_integration_v5_55_wireless.uf2) | Spatial ink check, approach-heading alignment and bounded endpoint assistance; physical retest pending. [Verification record](build-report-v5_55.json) |
| v5.54 — previous | [Download UF2](robocar_motion_integration_v5_54_wireless.uf2) | Recovery-direction handoff and slower, wider bounded rejoin verification; software checks passed, physical retest pending. [Verification record](build-report-v5_54.json) |
| v5.53 — previous | [Download UF2](robocar_motion_integration_v5_53_wireless.uf2) | 25 cm initial detour, completed-no-return tape rejoin, loaded-turn effort and retained failure reports. Straight-strip bypass observed complete; bend rejoin subsequently lost tape. [Verification record](build-report-v5_53.json) |
| v5.47 — historical | [Download UF2](robocar_motion_integration_v5_47_wireless.uf2) | Conditional faster cruise and transit; software checks passed, physical speed and straight-strip bypass tests pending. [Verification record](build-report-v5_47.json) |
| v5.46 — baseline | [Download UF2](robocar_motion_integration_v5_46_wireless.uf2) | One observed bend-obstacle bypass completed and resumed following. Full straight-strip return not yet demonstrated. [Verification record](build-report-v5_46.json) |
| v5.34 — historical | [Download UF2](robocar_motion_integration_v5_34_wireless.uf2) | Earlier barcode calibration checkpoint. [Verification record](build-report-v5_34.json) |

Target: Raspberry Pi Pico W / RP2040 on Robo Pico; GP28 centred over route tape,
GP27 to its right, GP26 dedicated to barcodes.

Hold BOOTSEL while connecting USB, then copy the chosen UF2 to RPI-RP2.
The robot starts stopped. Join the documented AAI10-Calibrate access point,
open http://192.168.4.1/. For the new barcode reader choose **Follow track + read
barcodes**, leave Y/Z straight, prepare, then use GP20 to start/stop.
See the [v5.63 measured-geometry test](../../docs/testing/barcode-v5_63-measured-geometry.md),
the [v5.62 recovery and barcode analysis](../../docs/testing/barcode-v5_62-recovery.md),
the [v5.61 barcode diagnosis and test](../../docs/testing/barcode-v5_61-motion-aware.md),
the [v5.60 straight-drift test](../../docs/testing/course-v5_60-drift-confidence.md)
and [earlier five-run analysis](../../docs/testing/course-v5_59-flow-bend.md).
For obstacle tests choose **Follow track + avoid obstacle**; that mode disables
barcodes. See [wireless instructions](../../docs/wireless-calibration.md)
and [obstacle test procedure](../../docs/testing/obstacle-v5_35-bypass.md).

v5.56 restores right-first alignment and scans before choosing the exit angle.
The leftmost servo check is followed by an optional10cm margin and full side
confirmation. Unknown right edges use the previous90-degree/25cm departure.
Known-edge plans include beam and chassis margins and may require a wider path.
Stable straight cruise can reach1000cps; rolling bend corrections remain slower
and bounded. Speed, obstacle clearance and rejoin behaviour need physical tests.

The v5.63 decoder is implemented and physically validated for the current home
AAZ layout: [two successful runs](../../docs/testing/barcode-v5_63-acceptance.md).
Universal reliability, the final mission and the µT-Kernel port remain unverified. Barcodes are disabled
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
