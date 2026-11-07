# µT-Kernel motion foundation

This separate Pico W target starts the real µT-Kernel scheduler and uses the
existing quadrature encoder driver and calibrated motion controller. It is a
USB bench-test build, not the combined line/barcode/obstacle/terrain mission.
The v5.63 target and accepted UF2 remain unchanged.

## What is implemented

- One Motion task (priority 2) requests a 10 ms delay between polls. Calibrated
  speed PI still updates at its existing 100 ms interval; endpoints are checked
  on every poll. Actual scheduling periods require board measurement.
- One lower-priority Observer task (priority 9) reads USB selections and emits
  a motion record every 200 ms. It cannot remotely start a run.
- A Pico hardware-alarm callback checks the raw GP20 stop input and 40 ms
  motor-service deadline every 5 ms, independently of foreground tasks.
- Starts require an explicit USB selection followed by a debounced GP20
  press/release. Releasing the abort button cannot restart motion. STOP cancels
  a prepared start; stale service tokens cannot re-enable outputs.
- Raw encoder counters retain the existing modulo-32-bit, never-reset
  behaviour. A consistent copy is taken with local interrupts excluded.
- Shared motion snapshots occupy 13 bytes, with explicit bit offsets, signed
  encoding and range checks. The existing native working controller is reused.
- Small new persistent control flags are grouped into masks: gate state uses
  five bits, board selection/readiness four bits, and button state three bits.

No Wi-Fi, MQTT, browser page, barcode capture, terrain estimator, servo or
obstacle controller is linked. Missing modules are labelled UNAVAILABLE in the
startup message; no invented sensor samples are presented as real data.

## Kernel and resource ownership

The startup/exception adapter follows Justin's existing `wifi-buddy-1` bridge
at `0a7604d807a7457a79c481a3a4758ac41fab45d7`. `port/mtk_bridge.c/h`
are copied unchanged from that commit. The dependency is the official
`tron-forum/mtk3_bsp` Pico BSP, pinned to
`15ed232c08f2d89e54e514db79de150e224307e3`.

This is a compatibility starting point for the team's µT-Kernel work. It is
not a migration to the separately supplied `sirfonzie/mtk3smp-rp2040` port.
Confirm the course's exact port/SMP expectations before final submission;
an exact-port requirement would require adapting this target's startup layer.

Pico SDK owns reset and GPIO/PWM/USB/timer hardware services. The bridge
installs µT-Kernel's SysTick and PendSV handlers. The kernel tick is 5 ms at
125 MHz. Core 1 is never started. Application callbacks do not call kernel APIs
from interrupts. All motor output writes, including the IRQ stop override,
remain inside `board.c`. The encoder IRQ remains the existing GPIO dispatcher;
GP20 is polled by the alarm, so no second GPIO callback replaces it.

`board.c` serializes its shared gate/selection/13-byte snapshots with local
interrupt exclusion. This protects one-core tasks and IRQs; **it is not SMP
synchronization**. Do not move it to two cores without changing that contract.
Only the local button path may call start/release APIs. Future communication
adapters must preserve that restriction.

The kernel has a 32 KiB pool, with two requested 4 KiB application task stacks
allocated by the kernel. Do not add those stacks to the pool twice. Main also
reserves a 4 KiB startup/IRQ stack. Runtime stack and allocation margins are
unmeasured. Task creation or hardware-timer failure permanently disables arming
until reset. Service epochs cover 1..65535 without recycling; exhaustion
requires reset, rather than accepting an old token.

## Build and check

Dependencies: the repository's Pico SDK 2.1.1/toolchain setup and the pinned
BSP. If the BSP is absent, download and pin it once:

```sh
git clone https://github.com/tron-forum/mtk3_bsp.git build/dependencies/mtk3_bsp
git -C build/dependencies/mtk3_bsp checkout --detach 15ed232c08f2d89e54e514db79de150e224307e3
sh tools/build_rtos_motion.sh
python3 tools/test_rtos_motion.py
```

The build helper accepts `PICO_SDK_PATH`, `PICO_TOOLCHAIN_PATH`, `MTK_BSP_PATH`,
`ROBOT_CMAKE`, `ROBOT_NINJA` and `ROBOT_PICOTOOL_DIR` overrides. Its default
picotool directory is the one already used by the calibration build.
Build output is `build/rtos-motion/robocar_rtos_motion.uf2`. Nothing is flashed
by these commands. Build/host evidence is in
[the foundation report](../../../docs/testing/rtos-motion-foundation.json).

The real `board.c`, runtime, safety gate and reused motion controller are
compiled together against mocked hardware for the host tests. They exercise
held-button boot, debounced local start, raw abort, no restart on release,
stop cancelling a prepared start, stale commands, non-finite PWM inputs,
service starvation, encoder faults, all four calibrated endpoints, counter
wrapping, invalid speed/count representation and initialization failures.
ASan/UBSan are enabled. A separate Python decoder checks signed/zero-containing
fixtures. ARM GCC measures the new structure sizes.

## First physical acceptance

This candidate has built and passed host simulation; **it has not run on the
robot**. Keep the accepted v5.63 UF2 available for ordinary track operation.

1. With the wheels raised, load this candidate and open USB serial. Confirm
   the `RTOS motion foundation v0.1` banner and recurring `[MOTION_V1]` records.
   Startup must leave the wheels stopped, including when GP20 is held at boot.
2. Type `f` to select the calibrated 30 cm test. Selection alone must leave
   the wheels stopped. After a stable release, press and release GP20 to start.
   Confirm both encoder signs/speeds and no resets or service trips.
3. Press GP20 while moving. Outputs must turn off; holding and then releasing
   the button must keep them off. A new press/release is needed for another run.
   Type `x` to check the USB stop path. It never starts a run.
4. Repeat the local-start/abort checks with `l`, `r` and `u` selections. Then
   measure loaded distance/turn accuracy on a suitable surface. Reused tuning
   does not establish the RTOS build's physical accuracy automatically.
5. Record request-to-PWM-disable traces, service gaps, timer/IRQ latency,
   stack margins and reset history. Exercise a deliberately starved task only
   with a controlled test build; do not infer a physical 50 ms stop guarantee
   from simulated time or the 40 ms guard setting.

The alarm design removes output at the next 5 ms check after the 40 ms deadline
while interrupts are serviced. Kernel interrupt masking, actual execution
times and mechanical braking remain part of hardware acceptance. It does not
claim to survive an arbitrary hard lock with interrupts disabled.

## Binary motion schema

Schema identifier: `motion/v1` (supplied by the eventual transport topic/type).
Bits are numbered least significant first within the byte stream:

| Field | Bits | Valid values |
| --- | --- | --- |
| Capture uptime in ms | 0–31 | Full uint32; wrap after 49.7 days; bounded unsigned age checks |
| Left relative wheel count | 32–47 | Signed -30000..30000 |
| Right relative wheel count | 48–63 | Signed -30000..30000 |
| Left measured speed, mm/s | 64–73 | Signed -500..500 |
| Right measured speed, mm/s | 74–83 | Signed -500..500 |
| Selected motion kind | 84–86 | 0 none; 1 straight; 2 left90; 3 right90; 4 right180 |
| Motion phase | 87–89 | 0 idle; 1 active; 2 done; 3 aborted; 4 fault |
| Controller fault | 90–92 | Existing motion fault codes 0..5 |
| Stop reason | 93–95 | 0 boot/none; 1 user; 2 service; 3 motion; 4 init; 5 range; 6 complete |
| Numeric measurement valid | 96 | Boolean |
| Output stop latched | 97 | Boolean, independent of the controller's latest phase |
| Byte padding | 98–103 | Zero; reject nonzero |

Invalid records zero all four numeric count/speed fields, retain time/state and
clear validity. Consumers must not treat those zeros as valid measurements.
Out-of-range data is rejected before narrowing. Capture uptime keeps the full
32-bit clock domain; a future shorter-session contract must be agreed before
reducing it. ±30000 relative-count bounds are explicit message limits and do
not change raw counters or the motion controller's arithmetic.

The 98 useful bits occupy 13 bytes. The equivalent native staging record is
28 ARM bytes; its wider inputs allow range checks before narrowing. This is a
**new-schema encoding comparison**, not measured savings for the entire old
firmware or a replacement for the multi-producer 603-byte JSON fixture.

USB emits the binary record as hex for inspection. Hex uses 26 printed
characters; this is not an MQTT binary transmission result. A future MQTT
adapter must publish the original 13 bytes with explicit length and include
the schema identifier. The independent receiver can decode saved USB logs:

```sh
python3 tools/decode_motion_record.py < motion-usb-log.txt
python3 tools/decode_motion_record.py --hex 00000000000000000000000001
```

## Remaining integration

The existing validated barcode implementation is untouched. Add its sampling
and low-priority decoding only after motor/stop scheduling is demonstrated.
Add Justin's binary MQTT transport and receiver with explicit lengths and A–Z
support; the A–D-only formatter defect remains in his unmodified branch.
Terrain, obstacle manoeuvres and barcode-command execution remain with their
owners. The whole-application minimum-bit inventory is still open; this target
implements compact motion exchange and control flags as the first increment.
