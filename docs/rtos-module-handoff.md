# RTOS module handoff draft

**Paused handoff,10 November:** Lionel and Ezra should begin with the
[consolidated project handoff](HANDOFF-2026-11-10.md). It distinguishes legacy
`main`, the accepted RTOS image, the latest uninstalled packing candidates and
the next integration/physical acceptance gates. The contracts below remain a
draft for team review.

Status: integration proposal, not an agreed team API. The current tested core is
RTOS v0.5.1 at `924eefe`, branch`yixuan/rtos-barcode-recross`, with two physical
A/A/Z passes (6/6), including one [live wireless run with full-A and Z recovery observations](testing/rtos-v0_5_1-wireless-track-acceptance.json).
v0.4.2 and v5.63 remain preserved rollback baselines. No teammate branch has
been merged. Preserve the working core while preparing adapters. Do not replace
working scanning with an entire divergent teammate branch.

## Ownership

| Work | Current boundary | Adapter needed when owner is ready |
| --- | --- | --- |
| Barcode command execution | Remains with the barcode execution member | Consume A–Z events with capture time and run ID; translate the agreed mapping into bounded Motion requests |
| Terrain/hump | Remains with the terrain member | Publish measured height and validity/freshness in agreed units; report failure independently of a valid zero |
| Obstacle manoeuvring | Pending Lionel's response and delivered code | Supply obstacle/bypass/rejoin state and bounded drive proposals through Motion; unknown/no-return must remain distinguishable from clear |
| RTOS, acquisition, motion arbitration and MQTT | Current integration branch | Keep one motor writer, one ADC owner and one network owner; preserve stops and complete-frame scanning |

No member must wait for another algorithm to finish before agreeing on their
input/output contract. No teammate algorithm has been ported or merged by this
document.

## Shared rules for adapters

1. `board.c` remains the only PWM writer. Modules propose motion; Motion
   chooses the permitted proposal. The existing latched STOP overrides all
   proposals, and stale run tokens cannot restart the car.
2. Do not add a second ADC loop. Current GP26/28/27 acquisition is IRQ-owned;
   line recovery and obstacle states must not suspend barcode acquisition.
   Any explicit scanner exclusion needs a documented physical requirement.
3. Keep raw quadrature counters modulo 32 bits and use consistent snapshots.
   Do not reset ISR counters to implement a relative manoeuvre distance.
4. Each published value carries a capture timestamp, validity, units/range and
   the relevant boot/run generation. Zero and unavailable are different states.
   Choose packed widths only after the owner confirms those domains.
5. Repeated A events at different capture times are distinct barcodes. Do not
   deduplicate by letter alone. Commands need an explicit replacement/defer
   policy while a manoeuvre is already committed.
6. Producers do bounded work and expose pending/complete/failed states.
   A driver that blocks while waiting for sonar, a sensor or a network reply
   cannot run in Motion, the ADC IRQ, or a decoder critical section.
7. Network connection/reconnection never starts movement. MQTT is an observer
   and STOP source; it does not own the drive loop. Real, simulated, stale and
   unavailable measurements remain distinguishable.
8. Keep this single-core µT-Kernel scheduler and SDK peripheral ownership until
   an explicit reviewed port change. Do not merge two kernel startups, SysTick
   owners, network stacks or core-1 designs by copying whole directories.

## Required delivery information

For each module, record the branch and exact commit, pins/peripherals/timers,
required polling period and measured blocking/execution time, data units and
ranges, public entry points, failure/stop behaviour, and its physical test log.
These are handoff requirements, not a claim that the module lacks such work.

Integration order follows the owners' readiness. Add one adapter, run its
host/board checks, then repeat the A/A/Z course before adding another. Terrain
telemetry can be integrated independently if it does not claim drive ownership.
Navigation and obstacle drive proposals need the arbitration policy first.

## Foundation work that can proceed now

- Record current linker, ARM layout and payload measurements (available in
  `testing/representation/20261109-rtos-v0_4_2-after.json`). The 1024-byte queue
  reduction is a same-semantics measurement; total-image BSS differences are
  not an equivalent-feature packing comparison.
- Runtime task stack watermarks, armed Motion/service timing and stop-source
  measurements now have real v0.5.1 course and earlier raised-wheel acceptance.
  Extend them when new producers are connected; retain the accepted image.
  Detection-to-PWM observations do not measure mechanical braking or free heap.
- Continue the range/ownership review for application state and work buffers.
  Compact MQTT alone does not complete Change Requirement 1.
- Full A during an observed recovery increment is now accepted (0→1); Z14→20
  within its capture also accepted in the wireless run. Repeat the same course
  after each adapter changes scheduling or drive ownership; final official
  course reliability remains a separate acceptance item.

The car can remain powered off while these software/evidence tasks are prepared.

## Minimum-bit handoff decisions to settle before merging

These are integration-review questions, not new assignments or changes to a
member's algorithm. No fixed height, range limit, precision or command mapping
is being invented on the owner's behalf.

| Owner/module | Required domain decision | Integration boundary |
| --- | --- | --- |
| Terrain member | Height units/resolution, signed vs unsigned, valid physical range, off-surface/failure/unavailable states and maximum age | Publish a timestamped observation; retain an explicit invalid state separate from zero. Validate a wide input before packing. |
| Lionel/obstacle | Range units/resolution, no-echo vs clear vs fault, bypass states, request duration and cancellation | Submit a bounded proposal to Motion; preserve its latched STOP/token and keep scanner acquisition running. |
| Barcode execution member | A–Z command mapping, repeated events, replace/defer/stop behavior and generation lifetime | Consume validated events; a same-letter later barcode is a new event. Do not start from network reconnect. |
| Justin/integration | Final topic/schema version, availability/simulation bits, payload maximum and receive validation | One Network task, explicit encoding, no raw C structs or unchecked truncation. |

For each field, record: valid range and number of states; required bits;
representation; input validation and overflow policy; producer/consumer and
locking; freshness; source commit and boundary tests. Keep a wider validation
argument when needed to reject out-of-range values before writing a packed
field. A bitfield cannot safely validate a value after it has already truncated.

The `0.6-dev3` packing of course/drive/search/trim state relies on Motion being
the sole writer on core0. An incoming module must not write those flags from
another task/IRQ. Route the request through Motion or review synchronization
first. The [current minimum-bit evidence](minimum-bit-conversion.md#stage-3-motion-owned-state-and-flags)
does not establish minimum-bit compliance of an undelivered teammate module.
