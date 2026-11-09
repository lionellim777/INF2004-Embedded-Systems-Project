# µT-Kernel barcode recross candidate v0.5.1

v0.5.1 is [built and software-tested](../../../docs/testing/rtos-v0_5_1-recross-fix.md)
on `yixuan/rtos-barcode-recross`; [installation and stationary USB/MQTT checks pass](../../../docs/testing/rtos-v0_5_1-stationary-acceptance.json).
It selects the latest
crossing of rescanned coordinates. The saved v0.5 full-A recovery failure now
decodes A without changing Code39 validators. Legacy/default behavior remains
unchanged. The candidate adds1024B scratch space, with the same task stacks.

The [first physical v0.5.1 A/A/Z run passes](../../../docs/testing/rtos-v0_5_1-track-acceptance.json):
all three retained attempts accepted, zero rejected captures and zero scanner
faults. Full A includes9deg yaw/11mm lateral travel and Z19deg/62mm. Live
telemetry was absent during the course and recovered on USB reconnection;
wireless power-handoff acceptance remains open. Next use the
[direct standalone-power startup](../../../docs/testing/rtos-wireless-start.md)
and verify it stationary before driving. No USB selection is required after
a fresh power-on. The console now prints live/waiting/silence status.

The [loaded v0.5 run](../../../docs/testing/rtos-v0_5-loaded-track-observations.json)
decoded half A and bend Z, but missed full A during recovery. Armed motor
service age stayed below10.5ms and all four low-stack markers were intact.
That earlier run is partial acceptance; the later v0.5.1 result is linked above.

v0.5 implements task stack observations, moving/idle timing and stop-source
measurements. See the [implementation and first hardware check](../../../docs/testing/rtos-v0_5-runtime-evidence.md).
It is installed and [stationary USB/MQTT stack observations are verified](../../../docs/testing/rtos-v0_5-stationary-acceptance.json).
The [first raised-wheel GP20 moving abort passes](../../../docs/testing/rtos-v0_5-gp20-stop-acceptance.json),
with fault-free latched stopping and a10023us armed Motion entry-gap maximum.
[Moving USB/MQTT aborts and stopped broker reconnection also pass](../../../docs/testing/rtos-v0_5-remote-stop-acceptance.json).
The next step is the [loaded A/A/Z timing and Decoder stack check](../../../docs/testing/rtos-v0_5-track-check.md).
The first-passing v0.4.2 image is preserved.

One [physical RTOS A/A/Z pass is verified](../../../docs/testing/rtos-v0_4_2-track-acceptance.json),
including Z during continued line recovery. Preserve this image/source while
preparing timing/stack measurements and the [module handoff draft](../../../docs/rtos-module-handoff.md).
Repeatability and full A during recovery remain open; no new flash is required
to record this acceptance.

This separate Pico W target starts the real µT-Kernel scheduler and uses the
existing quadrature encoder driver and calibrated motion controller. It is a
first combined line-following/barcode/motion/MQTT build. Terrain and obstacle
manoeuvres are not connected yet; the complete autonomous mission is pending.
The accepted v5.63 UF2 is preserved. Legacy builds keep the new recovery path
disabled; the default decoder behaviour is covered by the same regressions.

Use the [v0.4.2 recovery track instructions](../../../docs/testing/rtos-v0_4_2-barcode-recovery.md).
The saved v0.4.1 bend Z now replays successfully using the opt-in short-black
consensus path. An extra transition in full A still needs its raw capture;
this diagnostic build preserves the first substantial failed waveform.
The first moving v0.4 run decoded A/A but missed Z; it reported two rejected
captures, 33 recoveries and zero scanner faults. v0.4.1 adds stopped waveform
and per-attempt downloads, without changing the validated decoder/controller.
The [v0.4 build/test report](../../../docs/testing/rtos-v0_4-track-integration.json)
separates host pipeline tests, stationary Pico checks and pending moving-course
acceptance. Track mode is selected at normal boot; GP20 alone starts motion.

The v0.2 USB hardware check now has [recorded acceptance evidence](../../../docs/testing/rtos-v0_2-usb-acceptance.json):
bounded completion plus GP20/USB aborts, with no reported controller faults.
v0.3 adds the local Wi-Fi provisioning/broker console and network diagnostics.
Use the [next physical network checklist](../../../docs/testing/rtos-network-first-physical-check.md).
The [v0.3 setup validation](../../../docs/testing/rtos-v0_3-setup-validation.json)
records software-only checks; it does not claim a completed Pico radio test.
The subsequent [stationary Pico Wi-Fi/MQTT acceptance](../../../docs/testing/rtos-v0_3-stationary-mqtt-acceptance.json)
confirms association, DHCP, real binary motion telemetry and an empty STOP
acknowledgement after selecting the router's 2.4 GHz SSID. The subsequent
[live Pico restart check](../../../docs/testing/rtos-v0_3-broker-restart-recovery.json)
confirms telemetry recovery without a Pico reset and with the stop latch retained,
after fixing the host console's broker shutdown. User confirmation of physical
stopping under network traffic and measured timing/stack margins remain pending.
The [later delivery/input check](../../../docs/testing/rtos-v0_3-telemetry-delay-and-input-check.json)
distinguishes packet delays from actual reconnects and verifies buffered/repeated
command input. The radio requests no power saving; the SDK result is visible
over USB. Observed arrival gaps decreased, but timing guarantees remain pending.
The [final user network bench capture](../../../docs/testing/rtos-v0_3-user-network-bench-acceptance.json)
completes stationary networking acceptance: broker restart, live real motion
telemetry, STOP acknowledgement and saved fault-free captures. Physical loaded
motion, bounded timing and combined-mission integration remain separate checks.

## What is implemented

- One Motion task (priority 2) requests a 1 ms kernel delay between polls. Calibrated
  speed PI still updates at its existing 100 ms interval; endpoints are checked
  on every poll. Actual scheduling periods require board measurement.
- One lower-priority Observer task (priority 9) reads USB selections and emits
  a motion record every 200 ms. It cannot remotely start a run.
- One decoder task (priority 4, 8KiB requested stack) consumes a 14-byte/sample
  IRQ queue and runs the v5.63 grey waveform/geometry decoder with the opt-in
  RTOS short-black consensus path described in the v0.4.2 notes.
  The ADC IRQ remains the only owner of GP26/28/27 conversions: 2kHz sampling,
  the existing 12-sample filter and one retained reading every 2ms. Neither
  Wi-Fi nor line recovery suspends that acquisition. Run tokens discard stale
  results after abort/restart. Overflow and missing samples stop track motion.
- One Network task (priority 8) owns CYW43/lwIP/MQTT. The default candidate is
  unconfigured: the task sleeps without initializing the radio. Its complete
  network path is compiled and can be enabled with a local settings file.
- Normal v0.4 boot does not wait for a USB logger, and preselects track scan
  without moving. This permits normal power-bank operation. An explicit
  private `RTOS_WAIT_FOR_USB=1` can still request the earlier bring-up wait.
- A Pico hardware-alarm callback checks the raw GP20 stop input and 40 ms
  motor-service deadline every 5 ms, independently of foreground tasks.
- Starts require a debounced GP20 press/release; USB may change the preselected
  mode but cannot start it. Releasing the abort button cannot restart motion. STOP cancels
  a prepared start; stale service tokens cannot re-enable outputs.
- Raw encoder counters retain the existing modulo-32-bit, never-reset
  behaviour. A consistent copy is taken with local interrupts excluded.
- Shared motion snapshots occupy 13 bytes, with explicit bit offsets, signed
  encoding and range checks. The existing native working controller is reused.
- Small new persistent control flags are grouped into masks: gate state uses
  five bits, board selection/readiness/last-stop reason seven bits, and button
  state three bits. The network packs its period/state and control flags.

Binary Wi-Fi/MQTT support is linked; radio association requires local settings.
There is no calibration browser page, terrain estimator, servo or obstacle
controller in this target. Real line ADC records and accepted A-Z barcode reads
now feed MQTT. Missing sensor modules stay UNAVAILABLE. Y/Z actions remain
straight for the initial AAZ comparison; teammate command mapping comes later.

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

The kernel has a 32 KiB pool, with three requested 4 KiB stacks and one 8 KiB decoder stack
allocated by the kernel. Do not add those stacks to the pool twice. Main also
reserves a 4 KiB startup/IRQ stack. v0.5 observes application task stack markers;
loaded Decoder paths, startup-stack use and runtime free-pool margins remain
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
Default build output is `build/rtos-motion-v0_5_1/robocar_rtos_motion_v0_5_1.uf2`; network
setup builds separately under `build/rtos-network-v0_5_1/` using a private settings
header supplied by `RTOS_NETWORK_SETTINGS_FILE`. Nothing is flashed
by these commands. Build/host evidence is in
[the v0.2 report](../../../docs/testing/rtos-motion-v0_2.json).
The [v0.1 report](../../../docs/testing/rtos-motion-foundation.json) is a
historical record at source commit `80c3960`; its binary is preserved under
`build/rtos-motion-v0_1/`.

The real `board.c`, runtime, safety gate and reused motion controller are
compiled together against mocked hardware for the host tests. They exercise
held-button boot, debounced local start, raw abort, no restart on release,
stop cancelling a prepared start, stale commands, non-finite PWM inputs,
service starvation, encoder faults, all four calibrated endpoints, counter
wrapping, invalid speed/count representation and initialization failures.
ASan/UBSan are enabled. A separate Python decoder checks signed/zero-containing
fixtures. ARM GCC measures the new structure sizes.

For the guided USB helper and real loopback MQTT test, use the isolated tools
environment (already prepared on this workstation):

```sh
python3 -m venv build/dependencies/rtos-tools-venv
build/dependencies/rtos-tools-venv/bin/python -m pip install -r tools/requirements-rtos-tools.txt
python3 tools/test_rtos_track.py
python3 tools/test_rtos_runtime.py
build/dependencies/rtos-tools-venv/bin/python tools/test_rtos_network.py
build/dependencies/rtos-tools-venv/bin/python tools/test_rtos_bench.py
```

The network test runs the actual service state machine against SDK mocks and
independently decodes its packets. It also sends those C-produced packets
through a real loopback-only MQTT broker, restarts the broker, and verifies
resubscription and subsequent packets. Pico radio/network timing is still a
hardware test.

## Earlier USB bench acceptance

The v0.2 USB and v0.3 networking checks are complete within their recorded scopes.
Current moving-track acceptance uses the v0.4.2 guide above. Keep v5.63 available.
The steps below describe the original USB check; do not repeat them solely
because the software version changed. Follow the network checklist next.

Use the [specific first-test instructions](../../../docs/testing/rtos-motion-first-physical-check.md)
and `sh tools/run_rtos_bench.sh` to collect decoded logs automatically.

1. With the wheels raised, load this candidate and open USB serial. Confirm
   the `RTOS motion foundation v0.2` banner and recurring `[MOTION_V1]` records.
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

`[STOP_TRACE]` records a sequence, last stop reason, detected-request timestamp,
PWM-disable-write timestamp and maximum alarm interval. The write delay excludes
the interval from physical button press to its first firmware observation and
mechanical braking. It is not a complete physical stop-time measurement. The
helper saves raw logs and a JSON summary, refuses selections until recognizing
v0.2, and never sends a start command. Its exit stop still needs observation.

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
| Selected motion kind | 84–86 | 0 none; 1 straight; 2 left90; 3 right90; 4 right180; 7 track |
| Motion phase | 87–89 | 0 idle; 1 active; 2 done; 3 aborted; 4 fault |
| Controller fault | 90–92 | Existing codes 0..5; kind7 also allows 6, track supervisor |
| Stop reason | 93–95 | 0 boot/none; 1 user; 2 service; 3 motion; 4 init; 5 range; 6 complete |
| Numeric measurement valid | 96 | Boolean |
| Output stop latched | 97 | Boolean, independent of the controller's latest phase |
| Tail bits | 98–103 | Kind0..4: zero padding. Kind7: left upper3 at98..100, right upper3 at101..103 |

For kind7, join each count's existing low16 bits with its upper3 bits, then
sign-extend from bit18. The resulting signed19 count range is limited to
±160000 by the encoder/course contract. All other offsets stay unchanged.
Kinds5/6 remain reserved. Legacy kind0..4 bytes are unchanged; old receivers
reject unknown kind7 instead of silently decoding it with truncated counts.

Invalid records zero all four numeric count/speed fields, retain time/state and
clear validity. Consumers must not treat those zeros as valid measurements.
Out-of-range data is rejected before narrowing. Capture uptime keeps the full
32-bit clock domain; a future shorter-session contract must be agreed before
reducing it. ±30000 relative-count bounds are explicit message limits and do
not change raw counters or the motion controller's arithmetic.

The 98 useful bench bits or 104 useful track bits occupy 13 bytes. The equivalent native staging record is
28 ARM bytes; its wider inputs allow range checks before narrowing. This is a
**new-schema encoding comparison**, not measured savings for the entire old
firmware or a replacement for the multi-producer 603-byte JSON fixture.

USB emits the binary record as hex for inspection. Hex uses 26 printed
characters; this is not an MQTT binary transmission result. The MQTT
adapter publishes the original 13 bytes with explicit length and uses
the schema identifier. The independent receiver can decode saved USB logs:

```sh
python3 tools/decode_motion_record.py < motion-usb-log.txt
python3 tools/decode_motion_record.py --hex 00000000000000000000000001
```

## Binary MQTT adapter

The adapter follows Buddy1's async association/connect/subscribe/retry lifecycle
and single-network-owner policy, with the same bridge and adapted `lwipopts.h`.
It replaces JSON/sample stores with compact latest-value messages:

| Topic suffix under `inf2004/aai10/robot1/v1/` | Bytes | Representation |
| --- | ---: | --- |
| `motion` | 13 | Schema above; real encoder/controller data |
| `line` | 9 | Capture ms32, ADC12x3 in GP26/28/27 order, black pattern3, valid1 |
| `barcode` | 5 | Capture ms32, code5 (none/A–Z/unreadable), valid1, simulated1, zero tail1 |
| `status` | 10 | Capture ms32, boot ID32, valid-source mask5, simulated-source mask5, connection state3, zero tail3 |
| `link` | 1 | Numeric 0 unknown / 1 offline / 2 online; zero upper bits |
| `command_result` | 1 | Accepted1, command kind2, zero upper5 |

Source masks: bit0 motion, bit1 barcode, bit2 line, bit3 terrain, bit4 obstacle.
Simulation is a subset of valid sources. The initial barcode record is invalid,
with zero code; the new codec never substitutes `?` for Z. Barcode storage is a
latest snapshot, not a lossless event queue across network outages.

Every `mqtt_publish` receives an explicit byte length. SDK2.1.1's Last Will
interface accepts NUL-terminated messages, so the will is the one-byte binary
link code `0x01` on `link`, rather than a zero-containing status frame. No SDK
source was patched. Receivers must use `link` and sample age: a retained old
status packet is not evidence of a current connection.

Accepted command topics are `command/get_status` (empty payload),
`command/set_period_ms` (two little-endian bytes, range200..5000), and
`command/stop` (empty payload). Fragmentation is bounded to two bytes; wrong
lengths/ranges are rejected. STOP uses the local latch and cancels a prepared
start. **There is no remote START topic.** Reconnects never clear the stop latch.
Telemetry/results use QoS0; commands subscribe at QoS1. Duplicate STOP is safe.

Copy `network_settings.example.h` to the ignored `network_settings.local.h`,
enter the local 2.4 GHz Wi-Fi/broker settings, set `RTOS_NETWORK_CONFIGURED` to1
and rebuild for the later radio test. Keep passwords out of chat, git, logs and
command-line arguments. The first USB test needs none of these settings. The
default has empty credentials and does not attempt association.

The simpler operator route is now `sh tools/run_rtos_network.sh`. It prompts
locally for Wi-Fi settings, generates password-protected robot/observer accounts
with topic permissions, builds the actual configured v0.4.2 image and runs one
console for USB and MQTT evidence. `m` sends MQTT STOP and `b` restarts only its
own broker; it never sends START. Both source code and existing shared formats
use the updated receiver, which also accepts legacy bench records. Credentials and configured UF2s
stay under ignored `build/`; no passwords are logged. The software tests use
temporary synthetic settings and loopback brokers, not the user's Wi-Fi key.

`tools/rtos_mqtt_observer.py --broker <address>` decodes packets and resubscribes
after reconnect. It does not publish control commands. Use `--user` with
`--password-prompt` or a local `MQTT_PASSWORD` variable if required. The test
broker is loopback-only and is not the robot's LAN broker.

## Remaining integration

The v5.63 barcode implementation supplies the base decoder, with the opt-in
v0.4.2 exception above and independent acquisition. Full physical RTOS AAZ acceptance and
loaded control timing remain pending. The A–D-only formatter remains in Justin's
unmodified branch; this target uses the new A–Z codec.
Terrain, obstacle manoeuvres and barcode-command execution remain with their
owners. The whole-application minimum-bit inventory is still open; this target
implements compact motion exchange and control flags as the first increment.
