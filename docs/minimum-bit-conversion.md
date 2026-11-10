# Incremental minimum-bit conversion

Start conversion now, alongside module development. Keep the accepted v0.5.1
image and its two physical A/A/Z passes as the working reference. Do one
bounded representation change at a time, compare identical inputs/outputs,
measure ARM layouts and actual message sizes, then verify hardware timing
before accepting a new deployed baseline. Waiting for a whole-application
rewrite would combine unrelated range, overflow and concurrency changes.

The school change requirement covers application working data, sensing,
controller state, shared ISR/task data, queues and MQTT. Compact transport
alone does not complete it. Each field needs a justified range, width,
implementation and ownership/access policy, plus before/after evidence.

## How to use Tribunal 1

Preserve the completed Tribunal 1 report as the meeting record. Its proposed
ranges and representations must be checked against the actual implementation
and required behaviour. Record corrections and the reason in this working
decision log and final representation table; do not retrospectively claim a
proposal was implemented or validated. No submitted school document is edited
by this work. Any ambiguity in an assessed contract still needs clarification.

For example, none+A–Z+unreadable has28 states and requires5 bits. Encoder and
timestamp widths depend on their wrap/lifetime contract, not on the largest
value observed in one run. Wider arithmetic needs an explicit intermediate
bound/precision justification. Unresolved ranges stay marked unresolved.

## Parallel work boundaries

| Scope | Work now | Acceptance later |
| --- | --- | --- |
| Proven core | Convert bounded sensor buffers, flags and queue records in isolated steps; preserve existing precision and lifetime semantics | Host equivalence, target layout and then relevant hardware timing/regression |
| Teammate interfaces | Agree units, signed range, resolution, validity, freshness, overflow and single-writer ownership now | Review against each delivered algorithm; adapt and integrate one producer at a time |
| Teammate internals | Owners can develop and classify their own application fields while their algorithms are unfinished | Review actual code and measured limits without guessing ranges or rewriting their algorithms |
| Whole mission | Prepare arbitration and adapters around the existing stop/ADC/network owners | Combined mission, IRQ/task races, queue capacity, timing, stack/memory and final course evidence |

Do not pack fields owned by different tasks/IRQs into one shared word without
proving safe read-modify-write access. Do not narrow raw wheel counters or
absolute timestamps using an arbitrary assumed maximum mission duration.
Do not round geometry/filter/controller arithmetic merely to claim fewer bits.

## Stage 1 implemented as an opt-in candidate

Branch `yixuan/rtos-minimum-bit-state`, base`a4e15b5`.
`RTOS_COMPACT_ADC_STATE` defaults OFF, retaining the accepted implementation.
ON identifies itself as`0.6-dev1`; it is built for review and has not been
installed. The private installed-image metadata still points to v0.5.1.
The [stage1 measured build/test report](testing/representation/20261109-rtos-minimum-bit-stage1.json)
records the evidence and candidate hash. A separate default-OFF rebuild is
byte-identical to the accepted v0.5.1 binary, not merely the same version label.

The ADC filter is a useful first change because its input range and owner are
known: hardware ADC RESULT bits11..0, one IRQ writer,12 retained readings.
The old general-purpose filter remains untouched for legacy users. The new
candidate preserves every ADC bit, the12-reading rolling window, warm-up
division, floor rounding,2kHz input and existing2ms queue cadence.

| Stored field | Valid range | Bits | Representation |
| --- | --- | ---: | --- |
|12 history samples |0..4095 each |144 |18-byte array, explicit12-bit access |
| Rolling sum |0..49140 |16 |2 little-endian bytes |
| Next index |0..11 |4 |Low nibble of owner-only state byte |
| Filled count |0..12 |4 |High nibble of the same byte |
| Total | |168 |21 bytes, no padding |

The existing filter uses36 ARM bytes; the new one uses21, saving15 bytes in
that object. Linked BSS may change by a different amount due to alignment;
the candidate BSS is207316B versus207332B, a16-byte linked reduction from a
15-byte object reduction. The build report distinguishes these measurements.
Eight ADC conversions
per channel sum to at most32760, held in a16-bit arithmetic temporary. Raw
input outside0..4095 is rejected before averaging/packing and uses the existing
IRQ-safe `board_stop(STOP_RANGE)` path. It cannot truncate into a plausible
sample. The packed filter has no task writer and is initialized before its
timer is enabled, so its shared-nibble writes do not add an ISR/task race.

Validation uses the actual old filter as the oracle:1,102,600 updates covering
all4096 levels, warm-up, alternating rails and1,000,000 random readings.
Invalid inputs do not mutate filter state. Both default and compact modes run
the actual mocked ADC/queue/decoder/course/PWM integration. A/A/Z, stopped
partial, first-failure and maximum-size exports match byte for byte. Injected
bad ADC readings on each channel disable PWM through the range-stop gate.
ASan/UBSan and warning-as-error ARM builds are used. Hardware timing of this
candidate is pending; no complete minimum-bit compliance claim is made.

Run the software checks with:

```sh
build/dependencies/rtos-tools-venv/bin/python tools/test_rtos_minimum_bits.py
```

Build a private opt-in candidate in a separate output directory using
`ROBOT_COMPACT_ADC_STATE=ON`, `ROBOT_BUILD_DIR`, `ROBOT_OUTPUT_NAME` and the
existing private `ROBOT_NETWORK_SETTINGS`. Do not replace or flash the accepted
image until the candidate's intended hardware check is ready.

Next apply the same process to other bounded core state, and finish the
owner/domain matrix before touching motion deltas, timestamps or teammate
measurements. The largest diagnostic allocations need a separate optional
profile; removing debug data is a feature-budget change, not a bit-packing
saving. Keep those comparisons separate in the final evidence.

## Stage 2 prepared and software verified

[Measured candidates and validation](testing/representation/20261109-rtos-minimum-bit-stage2.json)
now include the stopped-line baseline and internal acquisition coordination.
`RTOS_COMPACT_CORE_STATE` defaults OFF. Enabling it with the ADC option builds
the full-diagnostic`0.6-dev2` candidate. Both options OFF, with the existing
first-failure cache ON, still produces a binary byte-identical to the accepted
v0.5.1. Neither candidate has been installed.

| Stored data | Proven domain and representation | ARM bytes before → after |
| --- | --- | ---: |
| ADC filter |12 ADC12 values, sum16, next4, count4 |36 →21 |
| Stopped-line baseline |31 clamped0..1000 levels x10 bits; count5; next5; unchanged full64-bit capture time |80 →48 |
| Acquisition coordination |ADC head/tail9 each, event head/tail3 each, enabled1, divider2 |13 separate-object bytes →4 |

The three object reductions total56 bytes; linked BSS is207276B versus207332B.
The four-byte acquisition container has27 application bits and five padding
bits. It uses internal target-verified C bitfields, never raw struct transport.
All task updates remain under local IRQ exclusion, and the ADC IRQ cannot be
preempted by a task. This proves the existing one-core ownership arrangement;
it is not an SMP synchronization contract. The two-bit divider explicitly
wraps0..3, rather than first storing4 and silently truncating it.

The centre baseline preserves input clamping,31-entry median/IQR logic,10ms
sampling spacing,100ms freshness and full64-bit time semantics. Its temporary
sorted buffer also stores10-bit values.120601 queries match the actual accepted
source at`a4e15b5`, including backward time and timestamps beyond32/48 bits.
The four independent ADC/core option combinations pass the real mocked
ADC/queue/decoder/course/PWM tests and produce identical diagnostic bytes.
Twelve alternating A/Z reads exercise both queue and event-ring wraparound.

Compiler frame reports show sample callback48→56 bytes, track begin136→136
and decoder service96→96. These are individual frames, not whole task/IRQ stack
bounds. Task stack requests, kernel pool and priorities are unchanged.
Candidate hardware IRQ/service timing and course regressions remain pending.

## Optional lean diagnostic profile

`ROBOT_FIRST_FAILURE_CACHE=OFF`/`RTOS_FIRST_FAILURE_CACHE=OFF` builds
`0.6-dev2-lean`. It omits only the duplicate first-failure raw cache:
32768B words +864B profiles +32B metadata =33664B. It retains the same decoder,
all attempt summaries and the latest frozen waveform. It therefore intentionally
cannot retrieve the first failed raw waveform after a later accepted scan.

| Profile | Linked BSS bytes | What changes |
| --- | ---: | --- |
| Accepted v0.5.1 |207332 |Physical reference with full diagnostics |
| Packed ADC/core with full diagnostics |207276 |56B representation reduction |
| Packed ADC/core with lean diagnostics |173612 |Additional33664B optional diagnostic-feature reduction |

The last reduction is a memory/feature-budget choice, **not a minimum-bit
packing saving**. The full-diagnostic candidate is the next hardware-check
candidate. Keep the lean variant available for later integration RAM needs.
Host tests verify the changed retention policy and that A/A/Z,12-event wrap,
stopped partial and maximum-size exports otherwise match the full profile.

## Stage 2 field inventory and remaining coverage at that checkpoint

The [current field checklist](testing/representation/20261109-rtos-stage2-fields.csv)
and [ARM measurement/source manifest](testing/representation/20261109-rtos-stage2-inventory.json)
cover47 selected declared application types and554 direct fields. This is a
declaration inventory, not554 live independent allocations or a completion
percentage. Containers/nested fields must not be summed. Included-but-unused
types, private C-file state and automatic locals still need allocation review.
Owner-review labels are suggestions, not team assignments or approval.

Only explicit checked domains are marked software verified.412 declarations
remain unresolved range/precision/lifetime/ownership items;95 Boolean and four
enum declarations have a known domain but still need storage/access review.
The four raw encoder counters keep their full modulo32 contract. A native
type alone is not accepted as a justification for a field's width. Wider
validation/staging values and arithmetic must be explained separately from
the checked internal/wire representation.

The auditor excludes private network settings and does not fetch, flash or run
the robot. Reproduce it with:

```sh
build/dependencies/rtos-tools-venv/bin/python tools/audit_rtos_representation.py
```

Repeat runs match for fields and measurements. Source hashes and compiler
sizes are saved. Normal network provisioning explicitly keeps the accepted
options OFF/cache ON; candidate-aware console identity gates are prepared,
with remote START still absent. Installed-image metadata remains v0.5.1.

## Stage 3: Motion-owned state and flags

The [10 November measured report](testing/representation/20261110-rtos-minimum-bit-stage3.json)
records the new full-diagnostic `0.6-dev3` and optional `0.6-dev3-lean` builds.
They are software-verified candidates, **not installed or physically accepted**.
`ROBOT_COMPACT_CONTROL_STATE=ON` enables the additional conversion; it defaults
OFF and normal network provisioning explicitly forces it OFF. The installed
v0.5.1 image, its metadata and the teammate algorithms are preserved.

This stage packs46 Boolean declarations across six controller types into
one-bit fields. Motion kind0..8 uses4 bits, phase0..4 uses3, fault0..6 (including
the RTOS track fault) uses3, and the stalled-wheel mask0..3 uses2. Together with
four profile flags, the main motion state occupies16 application bits. Course
phase0..7 and18 flags occupy21 bits in three bytes. Other controller numerical
fields remain at their existing precision, range and time/encoder semantics.
These declarations occur in nested/repeated allocations;46 is not a live-object
count or a whole-application completion claim.

| ARM object | Stage 2 bytes | Stage 3 bytes | Scope |
| --- | ---: | ---: | --- |
| `motion_wheel_t` |64 |56 |Two one-bit flags grouped; count/time/float fields unchanged |
| `motion_control_t` |176 |152 |Includes two wheels; do not add their savings again |
| `barcode_progress_t` |256 |248 |Twelve drive flags packed |
| `center_search_t` |144 |136 |Six recovery flags packed |
| `center_trim_t` |72 |72 |Four flags now use four bits; alignment prevents a size reduction |
| `course_control_t` |1160 |1064 |Includes two motion controllers and the above helpers |
| Whole `motion_runtime_t` |1432 |1312 |Includes course; net120B reduction for this stage |

Total representation savings relative to accepted v0.5.1 are now176B of linked
BSS: runtime152B (including the earlier baseline32B), ADC filter15B and capture
coordination9B. Full-diagnostic BSS is207156B, versus207332B accepted. Lean BSS
is173492B; its additional33664B reduction still represents removal of duplicate
diagnostic evidence, not packing credit. MQTT schemas/payload sizes are unchanged.

### Ownership and input validation

The Motion task is the only writer of these controller objects. The ADC IRQ,
Decoder and Network tasks do not update them. Observer reads the track status,
read count/letter and recovery counter under local IRQ exclusion; it does not
write the packed flags. The asynchronous stop guard still owns its separate
gate through `board.c`; packing does not move stop authorization into a course
flag. Thus no new word groups flags with separate task/IRQ writers. This is a
single-core owner proof, not an SMP synchronization design. Internal C bitfield
layout is measured with ARM GCC14.2 and never copied directly to a packet.

External `motion_begin` kind and distance arguments retain their original wide
types and rejection checks before internal storage. A kind17 request, for
example, is rejected; it cannot truncate into kind1 and start a straight test.
Boolean fields use `_Bool` semantics, so nonzero still means true. Enum growth
has compile-time guards; new values require reviewing widths. Raw counters,
absolute times, arithmetic temporaries and float control calculations have not
been truncated to make an unsupported compliance claim.

### Verification and next acceptance gate

`tools/test_rtos_control_bits.py` compiles the actual accepted control source
from`a4e15b5` and the candidate. A field-manifest check fails if a logical field
would disappear from the comparison.320 motion and240 course scenarios produce
90942938 identical canonical bytes, including exact float bit patterns, every
logical state field and string contents; padding and pointer addresses are
excluded. Invalid external kinds/distances, encoder wrap/faults, motor profiles,
abort/stall/late/timeout and line/barcode recovery are exercised. Eight existing
behaviour test programs add independently specified steering and safety checks.

All eight independent ADC/core/control option combinations pass the actual
mocked ADC-to-queue-to-decoder/course/PWM tests with identical A/A/Z, ring-wrap,
partial, first-failure and maximum-size exports. The lean profile differs only
in its documented retention policy. Host checks use ASan/UBSan; all three ARM
builds pass warnings as errors. Console identity gates recognize the candidates
while continuing to reject remote START. The normal provisioning test verifies
that inherited experimental flags cannot alter the accepted default.

An unchanged build made on10 November initially differed from the9 November
reference by two bytes in the SDK's automatic `__DATE__` metadata. Rebuilding
the default control with `SOURCE_DATE_EPOCH=1794225600` (9 November noon UTC)
produces an **exact** v0.5.1 binary match; no binary bytes are patched. Repeating
an exact-match build on another day requires the same date control.

Compiler frames for the sample IRQ remain56B vs48B accepted; course step is
192B vs184B, track begin136B unchanged, decoder service96B unchanged, Motion
runtime step128B unchanged and motion poll152B unchanged. Individual frames do
not establish task/IRQ stack bounds or loaded timing. The next gate is the
[candidate hardware check](testing/rtos-minbits-stage3-check.md), before treating
this representation as a deployed baseline or adding more scheduling changes.

The [updated field checklist](testing/representation/20261110-rtos-stage3-fields.csv)
and [ARM/source manifest](testing/representation/20261110-rtos-stage3-inventory.json)
still cover47 selected types/554 declarations:62 have candidate software
evidence,411 need range/precision/lifetime/ownership decisions,49 Boolean
declarations need storage/access review, four preserve raw-counter contracts,
and28 are containers requiring allocation review. Private state and automatic
locals still need review. Do not turn these counts into a compliance percentage.

Reproduce the software/measurement checks with:

```sh
build/dependencies/rtos-tools-venv/bin/python tools/test_rtos_minimum_bits.py
build/dependencies/rtos-tools-venv/bin/python tools/audit_rtos_representation.py
build/dependencies/rtos-tools-venv/bin/python tools/measure_rtos_minimum_bits.py
```

The last command reads the already-built private images and writes a public
measurement report under`build/`; it does not deploy, start motors or print
credentials. Team-member interface decisions can proceed independently of this
hardware gate, as listed in the [handoff draft](rtos-module-handoff.md).
