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
