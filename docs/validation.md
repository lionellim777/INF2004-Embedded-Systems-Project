# Verification record

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

These are acceptance steps. No Pico W was attached to the development session
that created this repository content, so no on-board results are claimed here.
