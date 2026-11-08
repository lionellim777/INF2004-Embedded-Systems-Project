# Minimum bit representation baseline and integration plan

Recorded 7 November 2026. Status: **before-change measurements complete for the
scope below; change-requirement compliance and RTOS integration remain open**.
The validated v5.63 firmware and decoder have not been modified or flashed. This paragraph
records the 7 November baseline, not current integration status.

Update, 9 November: the [RTOS after measurements](testing/representation/20261109-rtos-v0_4_2-after.json)
record actual v0.4.2 ARM layouts, linker allocations and real binary payloads.
[One physical A/A/Z pass](testing/rtos-v0_4_2-track-acceptance.json) now verifies
real line/barcode/motion/MQTT under the kernel. The same-semantics sample queue
uses7168bytes versus8192bytes, saving1024bytes. Whole-image RAM changes reflect
different features, including a temporary32KiB first-failure diagnostic cache;
they do not establish whole-application minimum-bit compliance. Runtime stack
and loaded stop/control timing remain open.

## Evidence and scope

Change Requirement 1 requires minimum-bit application representations throughout
the firmware, safe shared access, compact binary MQTT, and Week 13 repository
evidence of field ranges/widths, measured RAM/message changes, and correctness
and timing. The completed Tribunal 1 report, especially pages 12 and 19–20,
contains provisional decisions and pending actions, rather than implementation
acceptance. Preserve it as the meeting record; record subsequent corrections
with their reasons here and in implementation commits.

The checked-in measurements are:

- [Target measurements and synthetic MQTT fixtures](testing/representation/20261107-v5_63-before.json).
- [Field inventory with range decisions and proposed review owners](testing/representation/20261107-v5_63-fields.csv).
- [Audit tool](../tools/audit_data_representation.py).

The tool enumerates **658 direct fields across 59 application types** using
Clang, then measures sizes and member offsets with ARM GCC 14.2.rel1 for
Cortex-M0+. It does not use laptop `sizeof` for Pico layouts. Main's selected
structure sizes are checked against actual linked RAM symbols. The ELF's flash
image is byte-compared with the accepted UF2 and its sizes checked against the
v5.63 release report. The recorded release source hashes are also verified.

| Source | Pinned commit | Why inspected |
| --- | --- | --- |
| Main | `fefad265ff5aaf11a1de789b6937fdd09c1042da` | Accepted v5.63 baseline |
| `wifi-buddy-1` | `0a7604d807a7457a79c481a3a4758ac41fab45d7` | Existing µT-Kernel bridge, telemetry objects and serializer |
| `buddy4-terrain` | `4e9ba35ca69722ea56b75540cfd78a98bbbea690` | Existing packed terrain interface, read only |
| `buddy-5` | `4b8ad62b9d14b02dab632fd693661488aae7e747` | Existing compact sonar interface, read only |

The last row is the packed-data branch, **not** Lionel's newer manoeuvring
branch. No teammate's branch is merged. Hump implementation, obstacle
integration and barcode-command execution remain with their assigned owners.

Coverage includes named public records/enums/scalar aliases in the selected
headers, and the exact private `motor_t`, `button_t` and `waveform_sample_t`
declarations copied from main for measurement. Nested records are measured as
containers and in their own type rows; **do not sum both**. Fields on different
branches are separate baselines, not simultaneously allocated objects.

This is an initial inventory, not a declaration that every field complies.
Private C-file state, automatic locals and transport internals still need
range/lifetime review. Kernel/SDK internals are not classified as application
fields by this tool. Of the 658 rows, 402 explicitly need owner decisions;
other container and packed-owner reviews also remain open. Review-owner
labels suggest who should confirm a contract; they do not claim team approval.

## Measured RAM before changes

| Item | Bytes | Interpretation |
| --- | ---: | --- |
| Linked `.data` | 0 | Actual accepted ELF |
| Linked BSS/zero-initialized sections reported by `size` | 223,436 | Actual accepted ELF |
| RP2040 total SRAM | 270,336 | 264 KiB |
| Arithmetic remainder | 46,900 | **Not measured free heap or stack headroom** |
| Main stack reservation | 4,096 | From release configuration; no high-water measurement |
| Existing Wi-Fi branch kernel pool | 98,304 | Fixed 96 KiB array, separate build |
| Naive BSS plus kernel pool | 321,740 | Exceeds total SRAM by 51,404; **not a combined link result** |

Stack reservations and pools may already appear inside linked sections. Use
the final linker map and runtime high-water checks instead of subtracting or
adding them twice. The Wi-Fi branch's total BSS is not added to main's BSS:
shared network services and buffers must replace one another.

Largest relevant objects in the accepted ELF:

| Symbol | Bytes | Integration consequence |
| --- | ---: | --- |
| `waveform` | 73,596 | Working scanner plus diagnostics; not all disposable |
| `clients` | 37,008 | Three diagnostic HTTP clients; reconsider in a mission build |
| `core` | 26,640 | Diagnostic log/retained text; separate from mission data |
| `memp_memory_PBUF_POOL_base` | 24,515 | Network allocation; tune only against actual traffic tests |
| `ram_heap` | 16,403 | lwIP pool; already in linked RAM |
| `network_stack` | 8,192 | Existing core-1 network stack; reconcile with RTOS tasks |
| `waveform_queue` | 8,192 | Required sample transport in current scanner |
| `waveform_details` | 4,096 | Diagnostic text |

The scanner's frozen `saved` snapshot alone is 33,668 bytes, but is included in
`waveform`, not additional RAM. Live ADC/position arrays occupy 32,768 bytes
inside the same object. Design a lean mission diagnostic profile before
changing capture precision, capacities or the validated decoder algorithm.
No savings or acceptable final stack margins are claimed yet.

## Actual interface sizes before changes

These are target-compiled object sizes, including padding. Working controller
objects are not necessarily inter-task messages; the distinction matters when
reporting message savings.

| Type | ARM bytes | Role |
| --- | ---: | --- |
| `encoder_sample_t` | 16 | Four modulo/error counters |
| `waveform_sample_t` | 16 | Timestamp, both raw wheel counts and filtered ADC |
| `barcode_runs_t` | 73 | Existing 29 × 20-bit legacy run store |
| `motion_control_t` | 176 | Working controller state, not a proposed queue record |
| `course_control_t` | 1,160 | Working controller state |
| `telemetry_sample_t` | 32 | Existing producer copy including union/tag |
| `telemetry_event_t` | 20 | Existing event-queue record |
| `telemetry_snapshot_t` | 104 | Latest producer snapshots |
| `telemetry_wheel_history_t` | 108 | One history; both total 216 bytes on this ARM probe |
| Terrain branch `terrain_status_t` | 14 | Existing 110-bit packed status; not integrated |
| Sonar branch `obstacle_sample_t` | 2 | Existing 10-bit distance + 2-bit status in a word |

Enums measured with this ARM GCC configuration occupy one byte in these
types. Do not assume every C enum uses four bytes, or claim four-byte savings
from a host-only measurement. Future compiler/configuration changes require
fresh target measurements.

The real Wi-Fi branch serializer was compiled and executed on the host with
synthetic inputs, then independently decoded using Python JSON:

| Fixture | MQTT application payload bytes |
| --- | ---: |
| All producers invalid | 125 |
| Representative valid samples, barcode A | 603 |
| Same values with Z supplied | 603 |
| Representative scan event | 121 |

These are exact fixture lengths excluding the NUL terminator, topic and MQTT
protocol overhead. They are not worst-case sizes, network captures or hardware
validation. All representative producers are explicitly marked simulated.
The 768-byte formatter buffer is capacity, **not** measured message length.

**Confirmed integration defect:** `telemetry_write_line` permits only A–D.
The Z fixture serializes as `?`. Fix this in the shared protocol/adapter work;
the v5.63 scanner correctly accepting Z is a separate result. MQTT publication
also currently uses `strlen`; binary payloads need explicit byte lengths and
appropriate receive-side assembly/validation.

After-change RAM and message sizes remain **unmeasured**, not zero. They must
be collected from the revised implementation with equivalent fixtures and a
documented target build.

## Representation decisions to resolve

| Data | Defensible starting point | Required follow-through |
| --- | --- | --- |
| Boolean application flags | One bit per independent Boolean | Group by writer/ownership; protect shared word updates |
| Motion command distance | 0–1,000 mm needs 10 bits | Tribunal proposal differs from existing 1–700 mm bypass API; define command scope, zero and rejection policy |
| Commanded speed | Proposed ±150 mm/s needs 9 signed bits | Confirm every manoeuvre's supported speed; existing controllers use counts/s internally |
| Measured speed | Proposed ±500 mm/s needs 10 signed bits | Preserve sign; distinguish invalid/out-of-range from a valid zero |
| Barcode symbol | None + A–Z + unreadable needs 5 bits | Agree an explicit code mapping; three-bit A–D mapping loses Z |
| Raw ADC | 0–4095 needs 12 bits per sample | Preserve sentinel values where present; ADC sums/intermediates need their own bounds |
| Filter sum | 12 × 4095 = 49,140 needs 16 bits | Current 32-bit sum is a candidate; test full-scale/rolling replacement without altering filtering |
| Filter indices | 0–11 and 0–12 each need 4 bits | Distinct fields have distinct lifetimes; do not race shared updates |
| Legacy barcode runs | 0–750,000 units needs 20 bits | Already 73 bytes. Existing units include motion-domain values; do not substitute a temporal 10-bit ms format blindly |
| Raw wheel counts | Existing modulo-2^32 contract | Keep unchanged until all bounded-delta/wrap assumptions are reviewed |
| Manoeuvre-relative counts | Proposed ±30,000 needs 16 signed bits | Define start snapshot, manoeuvre/reset ID, maximum window, overflow and interrupted/restarted manoeuvres; do not reset the ISR counters |
| Time fields | Width depends on resolution, lifetime and wrap arithmetic | A 30-minute limit is a contract to justify, not an automatic reason to truncate every timestamp |
| Sonar | Existing packed branch has 10-bit mm + 2-bit status | Preserve unknown/no-echo distinctions and freshness; no-return must not become permission to drive |
| Terrain | Existing branch is already 14 bytes | Ezra owns physical validation and range decisions; no estimator change in this audit |

`CODE_RANGE` inventory rows describe current code domains. `RECODING_PROPOSAL`
requires a new mapping. `ADC12_ASSUMPTION` requires the stated sensor input
contract. Blank required-bit cells mean **unresolved**, not zero bits.
Packed bit-fields still need range review and an explicit portable wire format.
Wider arithmetic must be justified by its actual intermediate range/precision;
it is neither automatically exempt nor automatically safe to narrow.

## Shared access and timing requirements

1. One owner writes motor outputs. Network callbacks and sensor producers never
   drive motors directly. A latched stop must beat queued or stale commands.
2. Preserve the existing encoder snapshot semantics. Local interrupt exclusion
   only protects against local interrupts; it is not a cross-core mutex.
3. Preserve the sample IRQ's capture-time pairing of ADC and both wheel counts.
   Define queue ownership, overflow response and publication ordering before an
   RTOS/cross-core migration. `volatile` alone is insufficient.
4. Assign ownership of each fault bit, including who can clear it. RTOS event
   flags may notify a task, but do not automatically make all shared snapshots
   consistent. Check which operations are legal in IRQ context for the selected
   kernel port.
5. Copy shared snapshots under a short lock; release it before encoding,
   logging, MQTT or reconnect work. Reject stale samples and malformed commands.
6. Measure stop-request-to-PWM-disable separately from sensor-detection delay
   and physical braking distance. The tribunal's 50 ms target is not proven by
   the current 150 ms service guard. Earlier accepted runs observed about
   70–71 ms waveform work and 81–82 ms maximum loops; these are measurements
   from those runs, not new timing tests or worst-case bounds.

## Next implementation sequence

1. **Choose and pin the µT-Kernel base with Justin.** His branch already has a
   single-core BSP/SDK bridge. The supplied `sirfonzie/mtk3smp-rp2040` is another
   port with optional SMP. Determine whether the module requires that exact
   port/configuration; avoid silently combining two startup/scheduler systems.
   Reconcile SysTick, PendSV, IRQ ownership, core-1 use and network ownership.
2. **Create a lean, separate RTOS target.** Keep v5.63 and its UF2 available as
   the regression build. Establish the task/stack/RAM budget and motor-stop
   path before enabling vehicle motion. No second kernel pool or duplicate
   HTTP/MQTT network service should be added without a link-map measurement.
3. **Agree and implement one motion-data contract.** Real encoder snapshots
   feed Motion and consumers; use labelled simulated producers for unfinished
   areas. Define units, signed ranges, age, validity, reset ID and nonblocking
   queue-full behaviour. No teammate needs to finish their algorithm to agree
   this interface.
4. **Implement the versioned binary codec and receiver with Justin.** Preserve
   A–Z, publish explicit lengths, and reject invalid/reserved codes. Exercise
   zero bytes, negative values, boundaries, truncation and fragmented incoming
   payloads. Use identical semantic fixtures for before/after comparisons.
5. **Integrate one real producer at a time.** Test control deadlines during
   decoding and network reconnects. Add obstacle/terrain/navigation producers
   when their owners deliver them, then run physical course regressions.

No task priorities or execution periods are accepted solely because they appear
in a design table. Measure execution, lock holds, queue peaks and stack margins
under the intended sensor/network load before accepting the schedule.

## Reproduce the before measurements

Run from the repository with the ARM toolchain, Clang, a host C compiler, and
the preserved `build/motion-wireless/robot_firmware.elf` available:

```sh
python3 tools/audit_data_representation.py \
  --main-ref fefad265ff5aaf11a1de789b6937fdd09c1042da \
  --wifi-ref 0a7604d807a7457a79c481a3a4758ac41fab45d7 \
  --terrain-ref 4e9ba35ca69722ea56b75540cfd78a98bbbea690 \
  --obstacle-ref 4b8ad62b9d14b02dab632fd693661488aae7e747 \
  --output build/representation-audit
```

Override `--toolchain`, `--clang`, `--host-cc` or `--elf` if needed. On macOS,
set `SDKROOT` to the installed command-line-tools SDK if host compilation needs
it. The script reads existing local git objects; fetch missing commits first.
It never fetches, modifies branches, accesses credentials, flashes or runs the
robot. Output goes to ignored `build/` by default.

The tool is deliberately tied to the accepted v5.63 release for its **before**
ELF/UF2 integrity checks. Do not replace the checked-in before files with an
after build. Use a separate result and comparison harness for the future
integrated firmware; a mismatched ELF must not be presented as this baseline.

Validation of this audit: successful ARM probes and linked-symbol comparisons,
independent JSON fixture decoding, repeated-run output comparison, and rejection
of a deliberately modified ELF. No firmware rebuild, new physical test, RTOS
hardware acceptance, or before/after savings is claimed.
