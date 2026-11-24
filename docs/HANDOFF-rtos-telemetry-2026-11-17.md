> **Superseded.** This is a record from November 2026. The final firmware is described in [FINAL-FIRMWARE.md](FINAL-FIRMWARE.md).

# Handoff: RTOS terrain, hump and obstacle telemetry (2026-11-17)

**For:** Ezra (has the car). **Branch:** `rtos-terrain-telemetry` (from `main` after `6502220`).
**State:** host-tested only. Nothing on this branch has been built for ARM or run on the car.
The default build (`RTOS_TERRAIN=OFF`, `RTOS_OBSTACLE=OFF`) has the same topics and behaviour as accepted v0.5.1.

## What is done

| Piece | Where | Topic (under `inf2004/aai10/robot1/v1/`) |
| --- | --- | --- |
| Terrain snapshot record, 9 bytes | `binary_protocol.c` `terrain_record_encode` | `terrain` (QoS0, every motion period, status bit3) |
| Float → record converter for your `terrain_status_t` | `terrain_telemetry.c` `terrain_reading_from` | n/a |
| Hump event record, 8 bytes, plus an 8-slot outage queue | `binary_protocol.c`, `terrain_telemetry.c` `hump_queue_*` | `hump_event` (QoS1, one in flight, replayed after reconnect) |
| Obstacle state record, 10 bytes | `binary_protocol.c` `obstacle_record_encode` | `obstacle` (QoS0, motion period while active, status bit4) |
| Motion reports Lionel's bypass state each obstacle step | `track_runtime.c` `report_obstacle` | as above |
| Laptop decoder, broker ACL | `tools/rtos_mqtt_observer.py`, `tools/rtos_network_setup.py` | all three |
| Outage evidence in the session log | `tools/rtos_network_session.py` | `recovery_seconds`, `hump_*` in the summary |

Byte layouts are in `firmware/app/mission_rtos/README.md` (topic table) and `binary_protocol.h`.
Every layout has a hand-packed fixture checked in both the C and Python tests.

Rules you should know:
- Values out of range are **rejected, not clipped**. A rejected terrain or obstacle reading leaves the last published one in place, so its timestamp stops advancing.
- An invalid terrain reading (not available, not calibrated, or `pitch_valid` false) is published with `valid=0` and **all numbers zeroed, including the hump count and highest hump**. Tell me if you would rather keep the counts while the IMU is invalid.
- Hump queue: if 8 events are waiting (e.g. during a broker outage), the next one overwrites the oldest. Every later record carries `dropped` (cumulative, stops at 255).

## Your part 1: connect the terrain reader (needs your `ezra/ramp` code)

Nothing produces terrain data yet. Your detector is on `origin/ezra/ramp` (`68822af`).

1. Merge `origin/ezra/ramp` into this branch.
2. In `firmware/app/mission_rtos/CMakeLists.txt`, inside `if(RTOS_TERRAIN)`, add `"${BASELINE}/terrain.c"` to `target_sources` and link `hardware_i2c`.
3. Poll it from a low-priority task. **Recommended: the existing Observer task** (`main.c`, priority 9, 10 ms loop). A new task would also need a new `RUNTIME_*` id, its stack size and changes to the `runtime_diag` packet and decoder. In Observer, roughly:

```c
#ifdef RTOS_TERRAIN
#include "terrain.h"
#include "terrain_telemetry.h"
/* in observer_task, before the loop: */
static uint32_t humps_seen;
/* inside the loop, every 20 ms (hump_roll assumes 20 ms samples): */
terrain_poll();
terrain_status_t st=terrain_get_status();
terrain_source_t src={.available=st.available,.calibrated=st.calibrated,.sample_valid=st.pitch_valid,
    .roll_change_deg=st.current_roll_change_deg,.humps=st.roll_hump_count,
    .last_height_mm=st.last_roll_hump_mm,.highest_height_mm=st.highest_roll_hump_mm};
terrain_reading_t r;
if(terrain_reading_from(now,&src,&r)){
    (void)network_terrain(&r);
    /* '>' not '!=': terrain_calibrate() resets roll_hump_count to 0. */
    if(st.roll_hump_count>humps_seen && r.valid)
        (void)network_hump_event(now,(uint8_t)r.humps,r.last_height_dmm);
}
humps_seen=st.roll_hump_count;
#endif
```

What I checked in your `terrain.c` (`68822af`):
- It includes `hardware/i2c.h` and `pico/stdlib.h`.
- `hump_roll.h` is header-only, so `terrain.c` plus `hardware_i2c` is all the build needs.
- `terrain_init()` and `terrain_calibrate()` block with `sleep_ms` (about 2 s for calibration). That is fine only at the lowest priority, which is another reason to use Observer.
- `terrain_calibrate()` zeroes `roll_hump_count`, so hump numbers restart at 1 after every calibration. The laptop log handles this: it treats events as duplicates only when both time and number repeat.

4. Decide where `terrain_init()`/`terrain_calibrate()` run (calibration takes ~2 s with the car still and level; it must not run while Motion is driving). Also decide where `terrain_motion_go()` is called when GP20 starts a run. I left both open because they depend on how you want the start sequence to behave.
5. `terrain.c` prints over USB. Check that this doesn't flood the Observer's `[RTOS_INFO]` and `[MOTION_V1]` lines that the laptop parses.
6. Pins: the GY-511 is on GP4/GP5 (Grove 3). Check nothing in `mission_rtos` (or the obstacle build's sonar/servo pins) already uses them.

Please confirm the record ranges fit what you see on the car:
- roll change ±25.5° in 0.1° steps;
- hump heights 0–51.1 mm in 0.1 mm steps;
- hump count up to 255.

## Your part 2: tests that need the car

Build and provision through the session console, so the image gets your Wi-Fi and broker settings. `prepare()` passes `ROBOT_TERRAIN`/`ROBOT_OBSTACLE` through to the build. **Don't use `--reuse` when switching flags**: it reuses the last image.

```bash
ROBOT_TERRAIN=ON ROBOT_OBSTACLE=ON sh tools/run_rtos_network.sh
```

Check `build/rtos-network-local/firmware-info.json`: `options` should read `{"obstacle": "ON", "terrain": "ON"}`. The image name and `firmware_version` still say 0.5.1, so `options` is how you tell the images apart. Put it in your test notes.

Also build once with both OFF and confirm it still compiles cleanly. The default image must stay unchanged:

```bash
sh tools/build_rtos_motion.sh
```

Run the representation audits against the new state (`humps` queue, `terrain_latest`, `obstacle_latest`, `last_echo`) and compare with `main`. I couldn't run them: they need the macOS ARM toolchain and the preserved ELF (see `docs/representation-audit.md`).

```bash
python3 tools/audit_rtos_representation.py
```

```bash
python3 tools/bit_budget.py
```

Then run the car tests with that console:

1. **Normal run:** in the MQTT log, check that `terrain`, `obstacle` (obstacle mode `o`) and `hump_event` appear and decode. The status mask should show bit3 when terrain is valid and bit4 when obstacle mode is active.
2. **Network loss:** during a supervised run, press `b` (broker restart) at least 3 times, including once while driving over a hump. In `mqtt-summary.json` record:
   - `recovery_seconds` / `max_recovery_seconds`: time from the first sign of the outage to fresh motion data;
   - `hump_missing` (should be empty unless more than 8 humps happened during one outage), `hump_dropped_reported`, and `hump_replays` (duplicates are normal for QoS1).
   Also test a real Wi-Fi drop: turn off the hotspot for about 10 s, then back on.
3. **Motor timing under loss:** from `runtime_diag` (in the summary under `runtime`), Motion's worst gap must stay under the 40 ms motor-service limit during the outages above. Record each task's stack high-water mark from the same data.
4. **Request slots:** lwIP has only 4 MQTT request slots. With both options ON, the motion tick sends motion, line, terrain and obstacle (4), and `hump_event` and `scan_diag` can also be in flight. In the log, check that motion is still about 5 Hz and that `runtime_diag` rows still arrive. If motion drops, tell me and I'll move terrain or obstacle to every other tick.

Write results into `docs/testing/` as usual. Please include the commit hash and the build flags.

## Host test status (Windows machine, 2026-11-17)

Run with clang/LLVM 23. All of these pass:
- `firmware/tests/mission_rtos/network_test.c` in all four variants: default, `RTOS_TERRAIN`, `RTOS_OBSTACLE`, and both. ASan/UBSan on. `tools/test_rtos_network.py` now builds and runs all four itself.
- `provisioning_preserves_default` (opt-in flags pass through and are recorded in `firmware-info.json`).
- `restart_and_recalibration`: a packet still in flight during `b` is not counted as a recovery, and a hump-number reset or reboot is not counted as a replay.
- `RTOS_TEST_OBSTACLE=1 python tools/test_rtos_track.py` and the default `tools/test_rtos_track.py`.
- The decoder checks in `tools/test_rtos_network.py`, and the ACL check in `tools/test_rtos_network_setup.py`.
- `outage_evidence`, `telemetry_health` and the telemetry part of `delays_and_command_input` in `tools/test_rtos_network_console.py`.

These could not run fully on that machine, for environment reasons only (they fail the same way without these changes):
- `test_rtos_network.py` needs `test_rtos_runtime.py` fixtures first, and that script expects the macOS ARM toolchain.
- `test_rtos_network_setup.py` fails on Windows at its POSIX file-mode check.
- `test_rtos_network_console.py` fails on Windows at its pipe/select step.
- The representation audits need the macOS ARM toolchain.

On your Mac, please run the full set in order:

```bash
python3 tools/test_rtos_track.py
```

```bash
RTOS_TEST_OBSTACLE=1 python3 tools/test_rtos_track.py
```

```bash
python3 tools/test_rtos_runtime.py
```

```bash
python3 tools/test_rtos_network.py
```

```bash
python3 tools/test_rtos_network_setup.py
```

```bash
python3 tools/test_rtos_network_console.py
```
