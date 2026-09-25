# Verification record

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
