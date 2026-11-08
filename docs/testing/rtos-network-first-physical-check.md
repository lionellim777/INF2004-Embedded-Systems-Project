# First Pico Wi-Fi and binary MQTT hardware check

The v0.2 USB run is accepted for observed RTOS boot, one bounded completion and
two user-stop events. See [the saved bench evidence](rtos-v0_2-usb-acceptance.json).
The next check connects the Pico to an authenticated MQTT broker on this Mac.
The car stays on its support; no track, barcode or hump is needed.

## Start the single-console setup

1. Keep both powered wheels free of the table, normal motor power available,
   and the USB data cable connected. Leave the old USB helper closed.
2. Connect this Mac to the home router used for the test. The Pico needs that
   router's **2.4 GHz network name**; the Mac may remain on the router's 5 GHz
   network if both bands share the same LAN. The router must permit the Pico
   and Mac to communicate. No internet service/cloud broker is required.
3. Run in Terminal:

   ```sh
   sh "/Users/cyix/Github/INF2004-Embedded-Systems-Project/tools/run_rtos_network.sh"
   ```

4. Enter the **2.4 GHz Wi-Fi name** and its password at the local prompts. A
   separate name ending in `_5G` or `_6G` is unsuitable for the Pico W, even
   if the router also supports 2.4 GHz. The password is
   hidden. Accept the displayed Mac broker address if it matches the current
   Wi-Fi interface. It was `192.168.1.9` when prepared; the tool detects it again.
   Do not paste passwords into chat.

The tool generates separate robot/observer broker credentials, writes private
settings under ignored `build/rtos-network-local/`, and builds the actual
configured firmware. It then starts the broker, subscriber and USB logger.
No motor START is sent. Anonymous clients and incorrect passwords are rejected;
the observer account can send commands but cannot forge robot telemetry.
The broker binds only the chosen Mac address and uses the configured topic ACLs.

## Flash and confirm stationary data

5. Finder reveals the generated file:
   `build/rtos-network/robocar_rtos_network_v0_3.uf2`.
   Flash **that generated file** using the normal BOOTSEL method. Keep the test
   console running. It reconnects after the Pico returns. This image contains
   local credentials: keep it in the ignored build folder, not in git/releases.
   This bring-up image waits for the USB console before starting the kernel so
   early startup messages are captured. Keep USB connected for this test.
6. Wait for both:

   - `Pico v0.3: online; error: none`
   - `MQTT: valid motion data received from the Pico.`

   The wheels must remain undriven. The source masks must identify motion as
   available and the other sensor producers as unavailable. Barcode, line,
   terrain and obstacle modules are not physically integrated into this target.
   If macOS asks for Python local-network access, allow it for this local test.

### If Finder does not show RPI-RP2

On 8 November the Mac enumerated an active RP2 Boot interface but would not
mount its RPI-RP2 disk. The older disk-not-ejected notification did not identify
the current cause. No disk formatting or firmware erase was needed.
The official Raspberry Pi picotool v2.3.1 Mac release was downloaded into
`build/dependencies/picotool-usb-2.3.1/` and its release SHA256 verified.
USB PICOBOOT read, full-flash backup, firmware load/verify and reboot succeeded
through the existing USB hub. The backup is under ignored
`build/rtos-network-backup/`; local flash receipts are under
`build/rtos-network-local/`. Finder is therefore optional for future flashing.
See [Raspberry Pi's picotool documentation](https://github.com/raspberrypi/picotool).

USB diagnostics now include `wifi_link`: `-3` authentication failure, `-2` no
matching Wi-Fi name found, `-1` connection failure, `0` down, `1` joining,
`2` awaiting an IP address, and `3` connected with an IP address. These are
the SDK's states and distinguish an absent 2.4 GHz SSID from a DHCP timeout.
Dual-band routers may advertise separate network names: a name ending in
`_5G` can identify only the 5 GHz radio, even when the router also supports
2.4 GHz. Choose the actual 2.4 GHz network name.

If USB does not return, release BOOTSEL, disconnect USB and battery for about
five seconds, then reconnect normally without holding BOOTSEL. If the console
reports an error or never receives MQTT motion, stop at this stationary stage
and send the saved logs; motor tests are not needed to diagnose association or
broker login failures. `network_error` distinguishes radio initialization,
Wi-Fi/DHCP timeout, connection failure, login refusal and subscription failure.

## Stop and recovery checks

7. Type `f`, Enter to select the bounded motor test. Press/release GP20 to
   start. Once the wheels turn, press/hold GP20 to abort, then release it.
   Drive must remain off. This repeats the physical stop with networking active.
8. While stopped, type `m` **without Enter**. Press/release GP20 to start a new
   run; as soon as the wheels turn, press Enter. The console should report a
   Pico MQTT STOP acknowledgement, followed by the USB aborted/latched state.
   Confirm drive is removed and remains off. Acknowledgement alone does not
   prove physical stopping.
9. While stopped, type `b`, Enter. The owned broker pauses for two seconds and
   restarts. Wait for automatic reconnection and
   `MQTT: motion telemetry resumed after the interruption.`
   The broker should report it restarted within a few seconds. The Pico can
   take about 30 seconds to reconnect because of its retry backoff; avoid
   repeatedly pressing `b` while waiting. A duplicate press during the pause
   reports that a restart is already in progress.
   No new physical start should occur. The boot ID/uptime should show the Pico
   did not reset. Optionally repeat this after starting a bounded run to check
   that local control continues while the broker is unavailable; GP20 remains
   the physical stop and `x` is the independent USB stop.
10. Type `q`, Enter. The tool requests USB/MQTT stops and saves files under the
    printed `serial-output/rtos-network-<time>/` folder:
    `usb.log`, `usb.json`, `mqtt.jsonl` and `mqtt-summary.json`.
    `broker.log` contains any broker startup errors. Quitting during the
    restart pause cancels it and saves the capture without awaiting a stuck
    broker shutdown.
    Confirm drive is off before lowering the car. Report whether drive stayed
    off after GP20/MQTT stop and during reconnection. The files can be read
    directly from this shared workspace; no manual upload is required.

The console supports selection and STOP, never remote START. Each run needs
GP20. Network failure does not authorize or restart motion. The standard
v5.63 track firmware remains unchanged and available for restoring normal use.

## Repeat sessions and evidence boundaries

Normal reruns offer to reuse settings and rebuild. If both settings and the
Mac address are unchanged, `run_rtos_network.sh --reuse` skips the setup/build;
it verifies the saved firmware and settings hashes before starting services.
If the Mac address changes, rebuild so the Pico has the correct broker address.
This means the Mac's Wi-Fi **IPv4 address**, not its hardware/MAC address.
The Pico also needs the newly built image flashed; rebuilding alone does not
change its installed broker address. The console displays the saved and current
IP addresses when they differ. Failure to detect an address is reported
separately and should be resolved by reconnecting the Mac's Wi-Fi before retrying.

On the evening of 8 November the Mac's address had changed from `192.168.1.9`
to `192.168.1.5`. Updating only the private broker setting, rebuilding/flashing
and running the actual `--reuse` console restored real Pico telemetry. The saved
Wi-Fi name/password and MQTT credentials were retained. See
[broker-address update evidence](rtos-v0_3-broker-ip-update.json).

The 8 November restart check exposed a host-helper shutdown hang with an open
TCP connection. The broker now runs as an owned child process with bounded
termination; parent exit also closes it. Tests cover live binary traffic,
an unfinished MQTT login, repeated restarts and cancellation. A subsequent
stationary Pico check confirmed broker restart, automatic telemetry recovery,
unchanged boot ID, retained stop latch and another accepted MQTT STOP. See
[restart recovery evidence](rtos-v0_3-broker-restart-recovery.json). This helper
fix does not require changing or reflashing the Pico firmware.

Before this physical test, software verification covers target compilation with
synthetic credentials, real loopback password/ACL tests, binary packet integrity,
broker restart, USB command gates and motor/stop host regressions. It does not
establish Pico radio operation, loaded motion accuracy, full mission completion,
runtime stack margins or a worst-case physical stopping bound.
