# Minimum-bit data representation: Week 13 evidence

Change Requirement 1 asks for four things in the repository: a data representation table, a RAM comparison, a
message size comparison, and evidence of correctness and performance. This page holds all four for the
`mission_rtos` image (µT-Kernel on the Pico W). Older stage reports stay as the history of how it got here:
[incremental conversion](minimum-bit-conversion.md) and [baseline audit](representation-audit.md).

## What changed in this pass (stage 4)

- The three earlier "opt-in" packing options (`RTOS_COMPACT_ADC_STATE`, `RTOS_COMPACT_CORE_STATE`,
  `RTOS_COMPACT_CONTROL_STATE`) are now **ON by default**. `-DRTOS_COMPACT_*=OFF` rebuilds the old layout, which is how
  the "before" numbers below were produced from the same source.
- The packing now also covers the obstacle-bypass controller, the course follower's last flag, and the
  Observer-owned terrain state (hump detector, tilt alert, motion classifier, terrain status).
- **Shared fault bitmask** (`fault_flags.[ch]`): motor stall, encoder fault, IMU fault, no sonar echo, Wi-Fi loss and
  MQTT loss are single bits in one 16-bit word (8 active + 8 latched since boot), updated by one interrupt-masked
  read-modify-write.
- **`track_status` is now binary.** It used to publish the status or stop reason as ASCII text (4 to 37 bytes). It is
  now a 3-byte record: status code (one byte, table in `track_status_codes.h`), active fault bits, latched fault bits.
- Fixed a dashboard decoder bug found on the way: the Python phase table lacked `AVOID_RESTORE_FORWARD`, so a failed
  detour (phase 26) was mislabelled or crashed the decoder.

## 1. Data representation table

"Bits" is what the value needs for its valid range. Internal C bit-fields are laid out by the compiler and are never
copied to a packet; wire records are packed explicitly LSB-first by encoder functions. Owners are single-writer unless
stated.

### Wire records (MQTT topic prefix `inf2004/aai10/robot1/v1/`)

| Topic | Field | Valid range | Bits | Where |
| --- | --- | --- | ---: | --- |
| every record | capture time (ms) | 0..2^32-1, wraps at 49.7 days | 32 | explicit LSB-first, `binary_protocol.c` |
| `motion` (13 B) | left/right counts | -30000..30000, track mode ±160000 | 16 (+3 high bits in track mode) | `motion_record.c` |
| | left/right speed mm/s | -500..500 | 10 each | |
| | kind / phase / fault / stop reason | 0..7 / 0..4 / 0..6 / 0..6 | 3 each | |
| | valid, stop latched | flags | 1 each | |
| `line` (9 B) | three ADC channels | 0..4095 | 12 each | |
| | tape pattern | 0..7 | 3 | |
| | valid | flag | 1 | |
| `barcode` (5 B) | code | 0 none, 1..26 A..Z, 27 unreadable | 5 | |
| | valid, simulated | flags | 1 each | |
| `status` (10 B) | boot id | 32-bit | 32 | |
| | available, simulated source masks | 5 bits each | 5+5 | |
| | connection state | 0..5 | 3 | |
| `terrain` (10 B) | roll change | -25.5..25.5 deg, 0.1 deg steps | 9 signed | |
| | humps | 0..255 | 8 | |
| | last / highest hump height | 0..51.1 mm, 0.1 mm steps | 9 each | |
| | motion state | 0..6 | 3 | |
| | tilt active; tilt alerts | flag; 0..15 saturating | 1; 4 | |
| `hump_event` (8 B) | hump number; dropped | 0..255; 0..255 saturating | 8; 8 | |
| | height | 0..51.1 mm | 9 | |
| `obstacle` (10 B) | active; range valid | flags | 1 each | |
| | bypass phase | 0..26 | 5 | |
| | servo angle | 0..127 deg | 7 | |
| | range | 0..4095 mm | 12 | |
| | detected / completed | 0..255 | 8 each | |
| `track_status` (3 B) | status code | 0 unknown, 1..200 | 8 | `track_status_codes.h` (generated) |
| | active / latched faults | 6 defined bits | 8 each (6 used) | `fault_flags.h` |
| `link` (1 B) | state code | 1 offline, 2 online | 8 | MQTT will message |
| `command_result` (1 B) | result code | 0..3 | 8 | |
| `barcode_actions` (14 B) | action per letter A..Z | 0..11 | 4 each (26) | |
| | last command result | 0..3 | 8 | |
| `runtime_diag`, `scan_diag` | diagnostic packets | per schema | packed | `runtime_measure.c`, `scan_diagnostic.c` |
| commands (`get_status`, `set_period_ms`, `stop`, `set_barcode_actions`) | period | 200..5000 ms | 16 | `binary_command_frame_t`: kind 2, active 1, expected 2, received 2 bits in one byte |

### Internal state (task and ISR data)

| Data | Valid range | Bits | Implementation | Owner / access |
| --- | --- | ---: | --- | --- |
| ADC filter: 12 samples, sum, next, count | 0..4095 each; 0..49140; 0..11; 0..12 | 168 | 21-byte packed object (was 36 B) | ADC IRQ only |
| Stopped-line baseline | 31 levels 0..1000 | 310 + counts | 10-bit packed levels, 48 B (was 80 B) | Motion only |
| Capture coordination (queue head/tail, event head/tail, enabled, divider) | 0..511 ×2, 0..7 ×2, flag, 0..3 | 27 | one 4-byte bit-field word (was 13 B) | tasks write under local IRQ exclusion; the IRQ cannot be pre-empted by tasks |
| Sample queue | 12-bit ADC + metadata | packed | 7168 B (was 8192 B) | ADC IRQ producer, decoder consumer |
| Motion controller: kind, phase, fault, stalled wheels, 4 profile flags | 0..8, 0..4, 0..6, 0..3, flags | 16 | `unsigned short` bit-fields | Motion only |
| Wheel and drive flags (`held`, `finished`, `started`, `launch_active`, and 12 more) | flags | 1 each | `bool x:1` | Motion only |
| Course controller phase + 19 decisions (incl. `curve_follow`) | 0..7 + flags | 22 | `phase:3` + 19 one-bit flags | Motion only |
| Obstacle bypass: 10 decision flags | flags | 10 | one-bit fields (new) | Motion only |
| Obstacle bypass: close count, moving-invalid count, clear checks, clearance adjustments | each only compared with 1, 2 or 3 | 2 each | `unsigned x:2`, saturating increment `SAT2` (new) | Motion only |
| Obstacle bypass: phase, next, resume phase; servo, scan counters, distances | 0..26; 0..125; 0..1200 mm | 5; 7; 4..11 | existing bit-fields | Motion only |
| Hump detector: phase, sign, below, need-clear, hold-off | 0..2; -1/+1; flags | 2; 2; 3 | bit-fields (new) | Observer only |
| Tilt alert: active, above, below | flags | 3 | one-bit fields (new) | Observer only |
| Motion classifier: state, candidate, 5 flags | 0..6 ×2; flags | 6 + 5 | bit-fields (new) | Observer only |
| Terrain status: available, calibrated, pitch valid, tilt active, motion state | flags; 0..6 | 4 + 3 | bit-fields (new); copied as a whole snapshot | Observer writes |
| **Fault mask**: stall, encoder, IMU, no echo, Wi-Fi, MQTT; active + latched | 6 flags ×2 | 16 | one `volatile uint16_t`; `fault_set/clear` under `save_and_disable_interrupts` (new) | any task or IRQ: Motion (stall, encoder, no echo), Observer (IMU), Network (Wi-Fi, MQTT) |
| Network policy word: period, state; flags | 200..5000 ms + 0..5; 16 flags | 16; 16 | masks and shifts | Network task only |
| Values not narrowed | raw wheel counters, absolute µs timestamps, float control state | 32 / 64 / 32 | native | see below |

### What is deliberately not packed

- **Raw encoder counts and absolute timestamps** keep their full modulo-2^32 or 64-bit contract. Their width follows the
  wrap behaviour, not the largest value seen in one run. Narrowing them would be an arbitrary lifetime assumption.
- **Float controller state** (speeds, PWM, steering, geometry in the follower and bypass) keeps its precision. Rounding
  it only to claim fewer bits would change control behaviour.
- **Diagnostic buffers**: the barcode waveform recorder (74,620 B, of which 33,668 B is the saved snapshot), the 32 KiB
  first-failure raw cache and the 32 KiB kernel pool are the bulk of the RAM. Their words are already 12-bit ADC plus
  4-bit time in 16 bits, and their export format is frozen for the existing capture tools. Dropping the first-failure
  cache is a feature choice, not bit packing (`RTOS_FIRST_FAILURE_CACHE=OFF`).
- **Long-lived diagnostic counters** in the obstacle controller (`scans`, `rejoin_attempts`, and similar) and
  `encoder_skips[2]` stay as native integers. They are printed or compared against values this pass did not prove
  bounded. They are the next candidates.

## 2. RAM usage comparison

Same source, only the three packing options differ. Linked sizes from `arm-none-eabi-size`; layouts from
`tools/measure_minbit_layouts.py` with the Cortex-M0+ compiler.

| Image | Flash (text) | RAM (bss) |
| --- | ---: | ---: |
| `rtos-obstacle-creep21` (last proven image, before this pass) | 488,252 | 209,136 |
| Packing OFF, current source | 491,516 | 209,136 |
| **Packing ON (default)** | **491,300** | **208,744** |
| Saved by packing | 216 | **392 B** |

Where the 392 B comes from, by object (Cortex-M0+, bytes):

| Object | Before | After | Saved |
| --- | ---: | ---: | ---: |
| `obstacle` (global, `obstacle_bypass_t`) | 1,976 | 1,784 | 192 |
| `runtime` (global, includes the course and motion controllers) | 1,480 | 1,320 | 160 |
| ADC filter | 36 | 21 | 15 |
| Stopped-line baseline | 80 | 48 | 32 |
| `hump_roll_t` / `motion_class_state_t` / `terrain_status_t` | 40 / 116 / 68 | 36 / 108 / 60 | 4 / 8 / 8 |

Nested types sit inside their containers, so the rows are not additive: the 392 B total is the linked-image
difference. The per-type table is in [the layout report](testing/representation/20261122-minbit-layouts.json).

Packing the application state saves little in absolute terms because most of the 209 KB is not application state:
74 KB barcode waveform, 32 KiB kernel pool, 32 KiB first-failure cache, 7 KiB sample queue, and lwIP/Wi-Fi buffers.
The earlier queue and ADC reductions (stages 1 to 3) were introduced behind the same options, so the OFF image
contains the original sizes.

## 3. Message size comparison

All MQTT payloads are compact binary (section 1). The firmware never sent per-value text except `track_status`, so
there are two comparisons: the one real before/after, and each record against the same fields as JSON text.

**Real before/after (`track_status`).** The old record was the status string, 4 to 37 bytes (mean 18.4 over the 200
statuses). The new record is 3 bytes and also carries the fault masks.

**Binary vs text** (real encoders, decoded by the dashboard decoder, re-serialised as compact JSON;
[raw data](testing/representation/20261122-message-sizes.json)):

| Topic | Binary (B) | Same fields as JSON (B) |
| --- | ---: | ---: |
| motion | 13 | 191 |
| line | 9 | 86 |
| barcode | 5 | 103 |
| status | 10 | 115 |
| terrain | 10 | 198 |
| hump_event | 8 | 82 |
| obstacle | 10 | 174 |
| track_status | 3 | 132 |
| **all eight** | **68** | **1,081** |

The JSON column includes the decoder's derived names (for example fault names and `phase_name`), so it is a generous
text baseline. Treat the ratio as an upper bound. The text-protocol baseline from the v5.63 firmware (603 bytes for
one combined record) is in `20261107-v5_63-before.json`.

## 4. Correctness and performance

### Behaviour is unchanged by the packing

`tools/test_minbit_layouts.py` compiles the real firmware sources twice (packing off and on) and runs identical
pseudo-random input through both. A hash of every output and state field must match.

| Subject | Inputs | Result |
| --- | --- | --- |
| Obstacle bypass controller | 2,000,000 steps: 600 random-sensor runs plus 420 phase-reactive detours with random perturbation. 219 detours completed, 20 of 27 phases reached | identical hash `73d5596a26397c54` |
| Hump detector, tilt alert, motion classifier | 300 runs of 4,000 samples: 2,205 humps, 753 wrong-side events, 33 too-short, 2,363 tilt events, 11,043 state changes | identical hash `178881810266fad0` |
| Shared fault mask | every bit alone, latch survives clear, undefined bits ignored, every update bracketed by interrupt mask and restore | pass |
| `track_status` table | every status string in the sources is in the table; the Python decoder reads the same header | pass |

Other host checks run on this change (GCC 16.2 on Windows; no sanitizers are available there): the
ADC/queue/decoder/course/PWM integration test under all four packing combinations (11 to 12 PASS lines each); the MQTT
service test in four option combinations, including a new check that `track_status` is a 3-byte record and that Wi-Fi
and MQTT loss are latched in the fault mask; the waveform (42 PASS) and course-main suites; the hump, tilt and motion-class Python ports (they do not compile the C, which is why the C differential runs above exist);
and the dashboard decoder tests (12).

### Timing and performance

- Compiler stack frames (`-fstack-usage`, packing off to on): `course_step` 192 to 184 B, `motion_poll` 184 to 176 B,
  `track_service` 96 to 88 B. Every other measured function is equal. Nothing grew.
- Flash grew 3.1 KB over `creep21` for the 200-entry status table and the fault code. Packing itself makes the image
  136 B smaller.
- **Not measured here:** loaded task timing, IRQ latency and the 5 ms control step on the car. Bit-field access adds a
  few shift and mask instructions on a Cortex-M0+. The stack and flash numbers show no growth, but only a track run
  proves there is no timing regression. The existing `runtime_diag` records (task max gaps, stack watermarks) give the
  before and after: compare a run of `rtos-obstacle-creep21` with `rtos-minbit-final`.

### Known gaps

- Hardware acceptance of the packed image (course run, timing) has not been done. Keep `rtos-obstacle-creep21` as the
  fallback.
- Failures that predate this change and were not fixed: `firmware/tests/obstacle_bypass_test.c` (stale expectations
  after the creep fixes; it fails at the approach-warning and fast-cruise asserts, identically with packing on and off),
  `foundation_test.c` via `test_rtos_motion.py` (`!armed()` at line 43), `center_bend_test.c`, and the Python tail of
  `test_rtos_network.py` (needs `amqtt`). The packing equivalence above does not depend on them.
- The `track_status` table is generated from string literals in the sources. A status string built at run time, or one
  not in the table, is reported as code 0 (`UNKNOWN`). Re-run `tools/gen_track_status_codes.py` after adding a status;
  the test above fails if the header is stale.
- The firmware reports `0.6-dev3`, which the tool allow-lists accept. `track_status` changed from text to binary
  without a topic version bump, so a receiver older than this dashboard decoder will reject it.

## 5. BARR-C coding standard review (changed files)

Manual review of the lines added or changed in this pass against the BARR-C rules (the standard itself is not in the
repo; the checklist below is the rule topics applied). Scope: the new and edited firmware sources
(`fault_flags.c/.h`, `binary_protocol.c/.h`, `network_service.c`, `motion_runtime.c`, `main.c`, `obstacle_io.c`,
`obstacle_bypass.c/.h`, `hump_roll.h`, `tilt_alert.h`, `motion_class.h`, `terrain.h`, `course_control.h`,
`control_representation.h`, generated `track_status_codes.h`). Host tests under `firmware/tests/` and the Python tools
are outside the standard's scope and were not reviewed. Code that was already in these files and not edited in
this pass was not reviewed either. No automatic checker was run.

Fixed in this pass:

| Topic | Finding | Fix |
| --- | --- | --- |
| Braces | `if (...) return;` and `else fault_set(...)` without braces, `for` with an unbraced `if` body | Braces on every new `if`/`else`/`for` body, one statement per line |
| Implicit boolean tests | `!strcmp(...)`, `active&~FAULT_ALL` used as a condition, `out.state&&strstr(...)` | Explicit `== 0`, `!= 0u`, `!= NULL` |
| Function-like macro | `SAT2(x)` evaluated its argument twice | Replaced by `static unsigned sat2_inc(unsigned)` with a named limit `SAT2_MAX` |
| Magic numbers | `2000u` re-publish interval, `0xFFu`, `>> 8`, `0x3Fu` in the fault code | `TRACK_STATUS_REPEAT_MS`, `FAULT_ACTIVE_MASK`, `FAULT_LATCH_SHIFT`; `FAULT_ALL` is the OR of the six bits |
| One declaration per line | Bit-fields declared as `bool a:1,b:1,...` | One bit-field per line in `obstacle_bypass.h`, `motion_class.h`, `terrain.h` |
| Static data naming | file-scope `word` in `fault_flags.c` | `fault_word` |
| Integer types | `unsigned i` loop counter | `uint32_t` |
| File header | `fault_flags.c` had none | One-line file comment |

After the fixes the firmware builds with no warnings for both layouts (`-DRTOS_COMPACT_*=OFF` and the default), and the
obstacle and observer equivalence hashes are unchanged (`73d5596a26397c54`, `178881810266fad0`).
Image: text 491,300 B, bss 208,744 B.

Deliberate deviations (kept, with the reason):

| Topic | Where | Reason |
| --- | --- | --- |
| Bit-fields | the packed state structs | They are the requirement (minimum-bit representation). Layout is compiler-defined, so it is measured on the target compiler (section 2) and proved behaviour-neutral by the equivalence tests. No bit-field is shared between tasks or with an ISR; each has a single writer (comments in the headers). |
| `unsigned` and `bool` bit-field types | the same structs | Plain `unsigned` and `_Bool` are the portable bit-field types; `uint8_t` bit-fields are implementation-defined. The one signed field is declared `signed int` explicitly. |
| Line width | all edited files | The project uses up to about 120 columns throughout, which is longer than the standard's recommended width. New lines follow the project's existing width. |
| Pre-existing style | `if(x)return y;` one-liners, unbraced bodies and multi-declarator bit-field groups in lines this pass did not touch (`barcode_drive.h`, `center_search.h`, `center_trim.h`, the older flags in `course_control.h`, most of `obstacle_bypass.c`) | Not changed, to avoid a large unrelated diff on proven code. A whole-tree pass is a separate task. |

## Reproduce

```sh
py -3 tools/test_minbit_layouts.py          # equivalence, fault mask, status table (needs clang or gcc)
py -3 tools/measure_minbit_layouts.py       # ARM struct sizes, packing off vs on
py -3 tools/measure_message_sizes.py        # real encoder payloads vs JSON
py -3 tools/test_rtos_barcode_actions.py    # dashboard decoder, including binary track_status
```

Build the two images from the same source (project root, SDK environment as in the session handoff). Add
`-DRTOS_COMPACT_ADC_STATE=OFF -DRTOS_COMPACT_CORE_STATE=OFF -DRTOS_COMPACT_CONTROL_STATE=OFF` for the "before" image:

```sh
cmake -S firmware/app/mission_rtos -B build/minbit-on -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=pico_w \
  -DRTOS_TERRAIN=ON -DRTOS_CURVE_FOLLOW=ON -DRTOS_LAUNCH_CAP=ON -DRTOS_TRIM_GAIN=OFF -DRTOS_OBSTACLE=ON
cmake --build build/minbit-on
```
