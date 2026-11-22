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
| | obstacle width | 0..63 cm, 0 not measured | 6 | formerly padding; record size unchanged |
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
| Barcode sampler (`enabled`, `candidate`) and sample statistics (`black`, `gap_fault`, `overflow`) | flags | 1 each | `bool x ROBOCAR_BIT1` (new) | sampling IRQ writes; foreground only with that IRQ excluded |
| Barcode capture: 6 stream flags, `distance_mode`, `independent_optical` | flags | 1 each | one-bit fields (new) | decoder (foreground) only |
| Barcode reader (4 flags), waveform recorder (10 flags), IR roles (3 flags) | flags | 1 each | one-bit fields (new) | decoder task only |
| Scan state (4 flags), terrain snapshot (3), sonar result (3), motion record (2) | flags | 1 each | one-bit fields (new) | snapshots built by one task and copied whole |
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
| Packing OFF, current source | 491,604 | 209,136 |
| **Packing ON (default)** | **491,540** | **208,736** |
| Saved by packing | 64 | **400 B** |

Where the 400 B comes from, by object (Cortex-M0+, bytes):

| Object | Before | After | Saved |
| --- | ---: | ---: | ---: |
| `obstacle` (global, `obstacle_bypass_t`) | 1,976 | 1,784 | 192 |
| `runtime` (global, includes the course and motion controllers) | 1,480 | 1,320 | 160 |
| ADC filter | 36 | 21 | 15 |
| Stopped-line baseline | 80 | 48 | 32 |
| `hump_roll_t` / `motion_class_state_t` / `terrain_status_t` | 40 / 116 / 68 | 36 / 108 / 60 | 4 / 8 / 8 |
| Barcode sampler / capture / reader / stats | 904 / 408 / 88 / 56 | 896 / 396 / 84 / 52 | 8 / 12 / 4 / 4 |
| `barcode_waveform_t`, `scan_state_t`, `ir_roles_t` | 73,596 / 52 / 3 | 73,588 / 48 / 1 | 8 / 4 / 2 |

Nested types sit inside their containers, so the rows are not additive: the 400 B total is the linked-image
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

## 5. BARR-C coding standard

The project write-up requires conformance to the Barr C Coding Standard. It states no scope, no waived rules and no
pass threshold, so this pass applies it to every project-owned C source under `firmware/app/` (88 files in
`mission_rtos/` and `robot_firmware/`, including the wireless calibration code and the port layer).

Not covered, and why:

| Excluded | Reason |
| --- | --- |
| `calibration_page.h`, `measurement_page.h`, `stationary_page.h` | Generated from the HTML pages |
| `track_status_codes.h` | Generated by `tools/gen_track_status_codes.py` |
| `dhcpserver.c/.h` | Upstream Pico SDK example |
| `lwipopts.h` | lwIP configuration |
| `network_settings.local.h` | Local credentials, never committed |
| `firmware/tests/`, `tools/*.c`, `docs/**/*.c` | Host test and probe code, not firmware |
| Pico SDK, lwIP, micro T-Kernel | Upstream dependencies |

How it is applied:

- **Layout** is generated by `clang-format` from the repository's [.clang-format](../.clang-format): 80 columns, four-space
  indent, no tabs, braces on their own lines, braces inserted around every `if`/`else`/`for`/`while` body. Run
  `clang-format -i` on a changed file before committing.
- **Mechanical rules** are checked by `py -3 tools/check_barr.py` (80 columns, tabs, final newline, unbraced control
  bodies, several declarators on one line, forbidden `auto`/`register`/`exit`/`abort`/`setjmp`/`longjmp`). It reports
  **0 findings** on the 88 files. `--list` prints each finding.
- **Rules a script cannot check** (explicit comparisons, magic numbers, one-statement-per-line, macro hygiene, naming
  of new code) were reviewed by hand on the code added in this pass.

Changes made:

| Topic | Finding | Fix |
| --- | --- | --- |
| Line width and braces | 5,872 lines rewritten (over 80 columns, or with unbraced bodies) | Reformatted every file; control bodies braced |
| One declaration per line | declarations such as `unsigned a, b;`, `bool a : 1, b : 1;`, declarations wrapped over several lines, and typedef-typed pairs such as `motion_control_t leg, saved_leg;` | Split into one per line (checked on wrapped declarations too) |
| Wrapped `#define` | Reflowing moved trailing comments of 14 object-like macros onto continuation lines, which broke two tests that read the headers | Comments moved above the define |
| Nested ternary | five-deep `?:` chain selecting the console motion in `robot_firmware.c` | `motion_for_key()` with a `switch` |
| Unbraced macro body | `WIDTH_REJECT` in `barcode_decoder.c` | Braced |
| Implicit boolean tests | `!strcmp(...)`, `active & ~FAULT_ALL`, `out.state && strstr(...)` in new code | Explicit `== 0`, `!= 0u`, `!= NULL` |
| Function-like macro | `SAT2(x)` evaluated its argument twice | `static unsigned sat2_inc(unsigned)` and `SAT2_MAX` |
| Magic numbers | `2000u`, `0xFFu`, `>> 8`, `0x3Fu` in new code | Named constants |
| Naming and types | file-scope `word`, `unsigned i` loop counter | `fault_word`, `uint32_t` |

Verification that the reformatting changes no behaviour:

- **Object code:** the RTOS image (119 objects) and the non-RTOS `robot_firmware` image (73 objects) were compiled
  before and after the layout change and every object's disassembly and data (build timestamps masked) was compared.
  All are identical except `robot_firmware.c`, whose only change is the `motion_for_key()` refactor.
- **Host tests:** `tools/test_*.py` (23 scripts) run on the previous commit and on this tree with the same compiler
  shim give the same result for every script (the failures that remain fail identically on the previous commit; see
  section 4). `test_rtos_network.py` still passes all 55 checks and `test_minbit_layouts.py` reproduces the same
  equivalence hashes.

Deliberate deviations (kept, with the reason):

| Topic | Where | Reason |
| --- | --- | --- |
| Bit-fields | the packed state structs | They are the requirement (minimum-bit representation). Layout is compiler-defined, so it is measured on the target compiler (section 2) and proved behaviour-neutral by the equivalence tests. No byte is shared by flags written from different contexts (see the owner column in section 1). |
| `unsigned` and `bool` bit-field types | the same structs | Plain `unsigned` and `_Bool` are the portable bit-field types; `uint8_t` bit-fields are implementation-defined. The one signed field is declared `signed int` explicitly. |
| `ROBOCAR_BIT1` macro | `control_representation.h` and the structs that use it | Expands to `: 1` in the compact layout and to nothing in the old one, so one declaration serves both layouts and `-DROBOCAR_COMPACT_CONTROL_STATE=0` restores the proven layout. |
| Rule-by-rule review of old code | code not changed in this pass | Layout, braces, line width and declarations now conform everywhere in scope. Naming, prefix and function-length rules of existing modules were not audited. |

## Reproduce

```sh
py -3 tools/test_minbit_layouts.py          # equivalence, fault mask, status table (needs clang or gcc)
py -3 tools/measure_minbit_layouts.py       # ARM struct sizes, packing off vs on
py -3 tools/measure_message_sizes.py        # real encoder payloads vs JSON
py -3 tools/test_rtos_barcode_actions.py    # dashboard decoder, including binary track_status
py -3 tools/check_barr.py                   # mechanical BARR-C checks over firmware/app (0 findings)
clang-format -i <changed .c/.h>             # BARR-C layout from .clang-format
```

Build the two images from the same source (project root, SDK environment as in the session handoff). Add
`-DRTOS_COMPACT_ADC_STATE=OFF -DRTOS_COMPACT_CORE_STATE=OFF -DRTOS_COMPACT_CONTROL_STATE=OFF` for the "before" image:

```sh
cmake -S firmware/app/mission_rtos -B build/minbit-on -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=pico_w \
  -DRTOS_TERRAIN=ON -DRTOS_CURVE_FOLLOW=ON -DRTOS_LAUNCH_CAP=ON -DRTOS_TRIM_GAIN=OFF -DRTOS_OBSTACLE=ON
cmake --build build/minbit-on
```
