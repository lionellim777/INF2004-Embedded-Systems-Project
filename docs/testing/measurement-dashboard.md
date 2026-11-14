# Measurement dashboard: encoder counts per mm and IMU orientation

## Measurements tab in the main car dashboard (preferred)

The same two measurements are a **Measurements** tab in the normal `Car testing` dashboard
(`calibration.html`); the Testing tab is unchanged. Build and flash the usual motion firmware:
`.\tools\build_measurement_diag.ps1 -Motion` (add `-Station` to join your own Wi-Fi), UF2
`build\motion-wireless\robocar_motion_dashboard.uf2`.

- **Idle only, read-only.** `measurement.c` never touches motor, servo or PWM pins. Nothing happens
  until the tab is opened (browser sends `m`); the car goes silent again after `e`, after 30 s without
  the page's 10 s keep-alive, or as soon as a run starts (GP20). GP20 is **not** reused: it still starts
  and stops the car, so use the on-screen buttons.
- **Commands** (web or USB serial, car idle): `m` activate/keep-alive, `e` off, `g` zero and start a
  distance run, `h` latch it, `p` capture the next IMU pose, `w` restart the 7 poses.
- **Transport.** The live readout is served by `GET /measure` (with every retained `[MEAS RESULT]`
  line) and is never written to the shared log, so test logs stay clean. Result lines are also printed
  over USB. The events board ignores them.
- **Verification so far:** firmware builds with `-Wall -Wextra -Werror`; `node tools/test_measurement_tab.cjs`
  runs the tab script against a stub DOM using the real first-session results. Not yet run on the car
  with the full motion firmware; check that the tab connects, the live numbers move, and a normal test
  still runs and stops with the tab closed.

The standalone motors-off build below still works and is what the first measurements were taken with.

A motors-off diagnostic build that reuses the existing local Wi-Fi dashboard transport
(`AAI10-Calibrate`, http://192.168.4.1/). It collects the two measurements needed for the
hump-height work that cannot be taken with a ruler:

1. **Encoder counts per millimetre**, per wheel, from hand-pushed runs over a marked distance.
2. **IMU orientation and level baseline**, a seven-pose check of the GY-511 axes and sign
   convention, ending with the real hump geometry (only the right drive wheel raised).

The same logic exists as two USB-serial programs, `firmware/tests/distance_calibration_test`
and `firmware/tests/imu_orientation_test`. Use either; the results mean the same thing.

## Safety

No PWM, motor or servo code is linked. GP8 to GP12 are driven LOW on every loop iteration.
Push or tilt the car by hand. Nothing in this build can drive the car.

## Build

The motion firmware and the stationary-barcode build are unchanged: the new code is behind
`ROBOCAR_MEASUREMENT_DIAG`, and the section sizes of both existing builds were identical before
and after this change.

Windows (Pico SDK 2.3.1 installed by the VS Code Pico extension). From the repository root:

```powershell
$p="$env:USERPROFILE\.pico-sdk"
$env:PICO_SDK_PATH="$p\sdk\2.3.1"; $env:PICO_TOOLCHAIN_PATH="$p\toolchain\15_2_Rel1"
$env:PATH="$p\cmake\v4.3.4\bin;$p\ninja\v1.13.2;$p\toolchain\15_2_Rel1\bin;$p\picotool\2.3.1;$env:PATH"
python -I -X utf8 tools\embed_calibration_page.py        # only needed after editing measurement.html
cmake -S firmware/app/robot_firmware -B build/measurement-diag -G Ninja -DPICO_BOARD=pico_w `
  "-Dpioasm_DIR=$($p -replace '\\','/')/tools/2.3.1/pioasm" `
  -DROBOCAR_WIRELESS_CALIBRATION=ON -DROBOCAR_MEASUREMENT_DIAG=ON -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_C_FLAGS=-mcpu=cortex-m0plus -mthumb -Wall -Wextra -Werror"
cmake --build build/measurement-diag --parallel 8
```

`-X utf8` matters: without it Windows decodes `measurement.html` as cp1252 and corrupts the
non-ASCII characters in the generated header. `-Dpioasm_DIR` points the SDK at the prebuilt
`pioasm.exe` so no host C++ compiler is needed. The UF2 is `build/measurement-diag/robot_firmware.uf2`.
macOS: `tools/build_measurement_diag.sh`.

On Windows the same steps are one command: `.\tools\build_measurement_diag.ps1` (access point) or
`.\tools\build_measurement_diag.ps1 -Station` (see below). It uses the SDK under `%USERPROFILE%\.pico-sdk`.

## Station mode: use your normal Wi-Fi while working on the car

By default the car hosts its own `AAI10-Calibrate` network, so a computer that joins it loses its
internet. Station mode makes the car **join the network your computer is already on** instead.
It is opt-in at build time and available for every wireless build, not only this one
(`-DROBOCAR_WIFI_STATION=ON` with `-DROBOCAR_WIRELESS_CALIBRATION=ON`).

1. Copy `firmware/app/robot_firmware/wireless/station_settings.example.h` to
   `station_settings.local.h` in the same folder and enter the network name and password.
   The `.local.h` file is git-ignored.
2. Build with `.\tools\build_measurement_diag.ps1 -Station` and flash the
   `build\measurement-diag-station\robocar_measurement_v1_station.uf2` it produces.
3. Open a serial monitor (115200 baud). The car prints `[WIFI] READY (station) ... open http://<address>/`.
   Open that address in the browser on your computer. The car also names itself `aai10-car` to the router.
   Send `?` over USB to print the line again.

Behaviour:

- **Fallback:** if the car cannot join within about 8 seconds (wrong password, network out of range,
  5 GHz only), it starts the normal `AAI10-Calibrate` access point at http://192.168.4.1/ and prints why.
  You cannot be locked out. Each boot away from the configured network costs those 8 seconds.
- **Reconnect:** if the network drops, the car re-joins every 5 seconds. A changed address is reprinted
  over USB. Losing Wi-Fi never starts or repeats movement (existing behaviour).
- **Access point build unchanged:** without the option the code and binary are identical to before
  (section sizes compared against the original).

Limits and cautions:

- Pico W is **2.4 GHz only** and supports **WPA2-PSK**. WPA2-Enterprise ("sign in with username") and
  WPA3-only networks do not work. A phone hotspot set to 2.4 GHz is the easy option. For a WPA2/WPA3
  mixed router try `STATION_WIFI_AUTH` `CYW43_AUTH_WPA2_MIXED_PSK` in the settings file.
- Some networks (campus, guest) **isolate clients**, so your computer cannot reach the car even though
  both are connected. Use a hotspot or home router in that case.
- There is no mDNS, so use the printed address, or find `aai10-car` in your router's device list.
- **The password is compiled into the UF2.** Never commit or share a station UF2 or the local settings
  file. `build/` is git-ignored; `firmware/releases/` is not, so keep station builds out of it.
- On a shared network, anyone on it can open the page. In this motors-off build that is harmless. In the
  motion builds the browser can only select a test or stop; starting still needs GP20 on the car.

## Use

1. Hold BOOTSEL, connect USB, copy the UF2 to `RPI-RP2`. Power the car from USB or the power bank.
2. Access-point build: join Wi-Fi **AAI10-Calibrate** (password **AAI10-drive**) and open
   **http://192.168.4.1/** (http, not https). Station build: open the address the car prints over USB.
3. Wait for **Connected · car readings live**. Live values update twice a second.
4. Distance: mark START and END with the drive-axle line on each mark, type the distance, put the
   car on START, press **Zero & start run**, push straight and slowly to END, keep it still, press
   **Latch run**. Repeat at least 5 times. The right-wheel counts/mm is the one to use for the hump.
5. IMU: follow the prompt for each of the seven poses and press **Capture pose** (or GP20 after
   choosing **GP20 = IMU poses**). Hold the car completely still for one second.
6. Copy the **Everything to send back** block, or press **Download results**.

## Output lines

| Line | Meaning |
| --- | --- |
| `[MEAS LIVE] ...` | Twice-a-second readout; not retained. |
| `[MEAS RESULT] seq=N KIND ...` | Result events (`DIST_RUN`, `IMU_POSE`, `IMU_LEVEL`, `IMU_REJECT`, `IMU_NEXT`, `IMU_RESTART`, `IMU_COMPLETE`). Retained by the car (8 KiB, oldest lines dropped first) and returned by `/result`, so a browser reload loses nothing. `seq` is unique per boot; the page rebuilds its tables from the events, so duplicates and replays are harmless. |
| `[MEAS INFO] ...` | Guidance. |

Counts are forward-positive: the left raw count is negated, the right is not (documented raw polarity).
A run is rejected if a wheel total is zero or negative, or if encoder transitions were skipped.

## What was and was not verified

Verified on the host: the firmware builds with `-Wall -Wextra -Werror` and no diagnostics;
`node tools/test_measurement_page.cjs` runs the page script against a stub DOM (parsing, counts/mm
maths, rejection, pose sequence/retry/restart, duplicate and out-of-order replay, partial lines,
rendering) and checks that every key and command the page uses exists in the firmware and in the
transport whitelist.

**Not yet verified on hardware or in a browser:** Wi-Fi/HTTP service in this build, encoder counting
alongside the foreground Wi-Fi poll, the GY-511 readings, GP20 behaviour, and the page rendered in
a real browser. Check first that the connection badge turns green, live numbers change when you
turn a wheel or tilt the car, and `imu=OK` appears in the live line.
