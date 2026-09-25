# Verification record

## Scoped LAN firewall rule on 2026-09-25

- After administrator approval, Windows Firewall rule `Buddy1MosquittoLAN`
  was created and read back. It is enabled, inbound, allow, Public profile,
  TCP local port 1883, program `C:\Program Files\Mosquitto\mosquitto.exe`,
  local address `192.168.0.163`, remote subnet `192.168.0.0/24`, and interface
  `Wi-Fi`. No broad port-only rule was added.
- The authenticated LAN broker continued running, and the observer received
  a fresh Pico `state:"online"` heartbeat after the rule was installed.
  If the laptop's WiFi IP or subnet changes, update the rule, broker listener,
  and firmware broker address together.

## Pico MQTT connection on 2026-09-25

- Created ignored `config/local/mosquitto.conf` bound to the laptop's WiFi
  address `192.168.0.163:1883`, using ignored `config/local/passwords` and
  `config/local/acl`. Added a separate `observer` account; its generated
  password is in ignored `config/local/observer_password`. The listener uses
  `allow_anonymous false`, and the ACL gives `buddy1` its robot topics and
  `observer` read and diagnostic command access.
- Mosquitto 2.1.2 accepted the Pico from `192.168.0.159` as client
  `inf2004-team1-robot1` with username `buddy1`. It acknowledged both command
  subscriptions and a retained status publication. The observer received
  `state:"online"` heartbeats and telemetry snapshots; three sampled
  `captured_ms` values were 366946, 367146, and 367346, 200 ms apart.
- The observer published `set_period_ms` commands for 500 and 200 ms. The
  Pico returned `accepted:true` results for both, and the status again
  reported `period_ms:200`. A broker restart caused the Pico to reconnect;
  subsequent online status reported `reconnects:14` with the same `boot_id`.
- The LAN broker is running as a hidden user process using the local config,
  alongside the default localhost-only Mosquitto service. It is bound only
  to the laptop's WiFi IP and will need restarting after a reboot or IP
  change. Windows classifies this WiFi network as Public. Attempting to add
  an inbound firewall rule limited to Mosquitto, port 1883, and the local
  subnet initially failed with `Access is denied`. The later administrator
  run and verified rule are recorded above.

## Credential firmware reflash on 2026-09-25

- Rebuilt `build/pico-current/buddy1_pico_w.uf2` after changing the ignored
  firmware configuration. CMake regenerated the build, the generated secrets
  header matched the local one by hash, and the 45-step incremental build
  passed with Pico SDK 2.3.1 and Arm GNU Toolchain 15.2.Rel1.
- The Pico entered `RPI-RP2` BOOTSEL mode, accepted the UF2, and returned as
  `COM3`. USB serial showed an initial `CYW43_LINK_NONET` timeout, a retry,
  `WiFi: link up`, and `demo=60 comm=2977 state=2 retries=1 error=2`.
  `state=2` is MQTT connecting. Later output showed
  `demo=80 comm=3987 state=2 retries=2 error=3`. Both tasks advanced.
- The new broker address and generated `buddy1` credential are now flashed.
  The Mosquitto service was running, but its port 1883 listeners were bound
  only to `127.0.0.1` and `::1`. No listener was bound to the laptop's WiFi
  address, so MQTT login and publication from the Pico remain unverified.

## Broker credential preparation on 2026-09-25

- Mosquitto 2.1.2's `mosquitto_passwd` created an ignored local password file
  at `config/local/passwords` with user `buddy1` and a generated password.
  The same credential and the laptop's current WiFi IP `192.168.0.163` were
  placed in ignored `config/comm_secrets.h`; no password was printed.
- A temporary Mosquitto listener bound to `127.0.0.1:1884` accepted an
  authenticated QoS 1 publication from `buddy1` and acknowledged it. The
  listener was stopped and its temporary configuration removed. This proves
  the password file works locally, but does not establish Pico MQTT access.
- At this checkpoint, the firmware on the Pico predated the credential
  changes. The later reflash is recorded above.

## WiFi link established after router compatibility change, 2026-09-25

- The router was set to a 20 MHz 2.4 GHz channel, automatic channel selection,
  802.11b/g/n/ax mixed mode, and WPA2-PSK[AES]. Windows reported the laptop
  connected to `TP-Link_ADBE` on 2.4 GHz with WPA2-Personal and CCMP.
- Without reflashing, Pico USB serial advanced to
  `demo=820 comm=41343 state=2 retries=12 error=3`, then retried. State 2 is
  MQTT connecting, reached only after `CYW43_LINK_UP`. The WiFi link therefore
  succeeded with the router's new settings; both tasks continued running.
- No laptop listener was present on TCP port 1883, no Mosquitto service or
  local broker configuration was found, and the laptop's WiFi network profile
  is Public. The ignored firmware configuration still has placeholder broker
  credentials and a broker IP different from the laptop's current
  `192.168.0.163`. `error=3` is therefore consistent with missing broker
  access. MQTT connection, heartbeat, and telemetry remain unverified.

## WiFi continuation on 2026-09-25

- The ignored `config/comm_secrets.h` is now present. Its SSID matches the
  laptop's connected network, and its WiFi password matches the laptop's
  saved profile; neither password was printed. The broker address
  differs from the laptop's current `192.168.0.163`, and the broker username
  and password remain placeholders. These values were checked without
  printing credentials.
- This host has Pico SDK 2.3.1, CMake 4.3.4, Ninja 1.13.2, Arm GNU Toolchain
  15.2.Rel1, and picotool 2.3.1 under `C:\Users\jwooh\.pico-sdk`. The pinned
  micro T-Kernel BSP commit `15ed232c08f2d89e54e514db79de150e224307e3`
  was fetched to ignored `vendor/mtk3_bsp`. A clean 253-step firmware build
  passed using the installed SDK and compiler, which differ from the
  previously verified 2.2.0 and 14.3.Rel1 versions.
- The resulting `build/pico-current/buddy1_pico_w.uf2` was flashed via the
  board's `RPI-RP2` BOOTSEL drive. `COM3` returned. New serial diagnostics
  showed WiFi deadlines with CYW43 link status `-1` and then `-2` on two
  attempts. `-2` is `CYW43_LINK_NONET` (no matching SSID found). The board
  still did not join, despite the laptop seeing the SSID on 2.4 GHz with
  WPA2-Personal and CCMP. MQTT cannot be tested until WiFi joins and the
  broker settings are populated.
- After the router was changed, Windows reported `TP-Link_ADBE` on 2.4 GHz,
  channel 8, as WPA2-Personal with CCMP. A wireless scan also showed that
  SSID and its 2.4 GHz BSSID. The laptop retained `192.168.0.163/24`.
  The Pico's existing firmware continued through another association attempt:
  `demo=1740 comm=87798 state=1 retries=22 error=2`, then
  `demo=1760 comm=88808 state=5 retries=23 error=2`. Thus the router change
  alone has not established the link. The flashed SSID and password cannot be
  verified from this checkout, and this image predates the new link-status
  diagnostic messages.
- The Pico W was present as `VID_2E8A` on `COM3`. With DTR asserted, its USB
  serial output included `demo=920 comm=46398 state=5 retries=13 error=2`.
  The tasks are still running, but the board remains in the WiFi retry state.
- The laptop's current `TP-Link_ADBE` connection is on 2.4 GHz, channel 6,
  with WPA3-Personal (H2E). The firmware requests WPA2 AES PSK. The access
  point needs to advertise a WPA2-Personal compatible 2.4 GHz network for
  this firmware. The laptop's current WiFi IPv4 address is `192.168.0.163`;
  recheck it when configuring the broker because it can change.
- This checkout has no `config/comm_secrets.h`, Pico SDK/BSP environment
  variables, or Pico build tools. The connected board runs an earlier image.
  No new firmware was built or flashed in this continuation. The firmware
  now prints association start, link up, and CYW43 link status at the WiFi
  deadline to make the next hardware check conclusive.
- MinGW GCC host protocol test, independent JSON contract check, and the
  mechanical BARR-C checker passed. The system Python launcher points to an
  unavailable WindowsApps interpreter, so the checks used Codex's bundled
  Python runtime.

At this checkpoint, the next step was to check the router's 2.4 GHz
compatibility settings. The subsequent result is recorded at the top of this
file.

## Hardware and build check on 2026-09-25

- With the local WiFi credentials configured, the Pico W booted and both
  tasks continued to run. A temporary diagnostic build reported CYW43 link
  status `-2` (`CYW43_LINK_NONET`, no matching SSID found) at the 15-second
  WiFi deadline, then entered its retry state. This points to the configured
  access point being unavailable to the board in the current location; it
  does not establish WiFi or MQTT operation.
- Pico USB serial output was visible when the terminal asserted DTR. Earlier
  empty captures without DTR were a capture issue, not evidence of a stopped
  scheduler. The temporary diagnostic logging was removed after this check.
- The laptop broker accepted an authenticated local diagnostic publication
  on its LAN listener, but no Pico-originated MQTT connection or heartbeat
  was seen. Recheck the laptop LAN IP in the ignored firmware configuration
  and rebuild/reflash after moving the board to the target 2.4 GHz network.

- Pico W firmware built with Pico SDK 2.2.0, Arm GNU Toolchain 14.3.Rel1,
  Visual Studio host C++, and the pinned micro T-Kernel BSP. ELF and UF2 were
  produced. A fresh `build/pico-clean` configuration and 248-step build also
  passed. `arm-none-eabi-size` reported 385,584 bytes text and 157,676
  bytes BSS for the latest build. Stack margins are not yet measured.
- Windows identified the connected board as `VID_2E8A` and `COM4`. It entered
  `RPI-RP2` BOOTSEL mode and accepted the UF2. USB serial output from the
  flashed board included `demo=40 comm=1967 state=1` and, about ten seconds
  later, `demo=60 comm=2977 state=1`. Both RTOS tasks therefore advanced;
  the communication task ran about 101 iterations per second in this sample.
- State 1 is WiFi connecting. The example SSID and password were still in
  use, so this is not evidence of WiFi, MQTT, heartbeat, or recovery.
- A subsequent UF2 with retry diagnostics produced
  `demo=40 comm=1967 state=1 retries=1 error=2` and
  `demo=100 comm=4993 state=5 retries=3 error=2` over roughly 30 seconds.
  The demo and communication tasks kept advancing while failed WiFi joins
  entered the retry state. Real link-loss and broker recovery are untested.
- The final clean-build UF2 was also flashed. `COM4` returned and reported
  `demo=40 comm=1967 state=1 retries=1 error=2`, followed about ten seconds
  later by `demo=60 comm=2977 state=1 retries=1 error=2`.
- Host Ninja build and `ctest` passed two tests, including fragmented,
  incomplete, oversized, and out-of-range diagnostic commands and independent
  JSON parsing of maximum-value telemetry. The mechanical BARR-C checker and
  GCC static analyzer passed. These checks do not establish full standard
  compliance.
- Mosquitto 2.1.2 was installed on the laptop. A temporary listener bound to
  `192.168.0.202:1883`; authenticated command publish succeeded, an observer
  read a retained test status, and the broker log denied an observer publish
  to the robot-only status topic. The temporary LAN listener was stopped after
  this local test. No Pico-originated broker message has been observed yet.
- The laptop's `Ethernet 2` network is currently classified Public. The
  installer created broad Public-profile Mosquitto firewall rules, and this
  session lacked permission to narrow them. Keep the LAN listener stopped
  until firewall scope is reviewed for the chosen private network.

## Checkpoint on 2026-09-24

- `python tools/check_barr.py`: passed mechanical checks.
- Host Ninja build and `ctest`: passed (1 protocol test).
- Pico W CMake configuration: completed with Pico SDK 2.2.0 and the pinned
  micro T-Kernel BSP.
- Pico W build: blocked while configuring the SDK's host-side `pioasm` tool;
  CMake reported `No CMAKE_CXX_COMPILER could be found`. No ELF or UF2 was
  produced. Re-run with a working host C++ compiler before claiming a firmware
  build. Application and hardware behavior remain unverified.

Host checks should be run after every protocol change. Record the command,
tool version, date, and pass/fail output. A host result does not establish Pico
W or integrated car acceptance.

## Hardware demonstration checklist

1. Record the Pico W revision, SDK/toolchain/BSP revisions, ELF and UF2 names,
   map size, and USB diagnostic output showing both task counters advance.
2. Start the private Mosquitto broker and a timestamped subscriber. Confirm
   retained `online` status, a one-second heartbeat, and periodic telemetry.
3. Request status and publish 200, 500, 5000, 199, and 5001 as diagnostic
   periods. Record accepted or rejected command results and observed intervals.
4. Stop and restart the broker; repeat with WiFi temporarily unavailable.
   Record reconnect count, restored subscriptions, fresh telemetry, and any
   dropped events. Confirm autonomous control continues.
5. Force an unexpected Pico disconnect. Confirm Mosquitto publishes `offline`
   from the Last Will after its detection period.
6. Repeat ten reconnect cycles and a 30-minute run. Record stack margins,
   queue peak, linked memory use, control deadline misses, and any resets.

The completed Pico W checks above establish boot, task scheduling, and WiFi
retry without a working access point. The remaining acceptance steps need a
configured network, broker access from the board, and the integrated car.
