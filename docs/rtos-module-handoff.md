# RTOS module handoff draft

Status: integration proposal, not an agreed team API. The current baseline is
RTOS v0.4.2 at `916c0dc`, with one verified physical A/A/Z pass. Preserve that
build while measuring the foundation and preparing adapters. Do not replace
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
- Prepare low-overhead runtime stack/headroom, Motion-service and stop-delay
  measurements in a separate candidate. Keep the accepted v0.4.2 image intact.
  Distinguish detection, PWM disable and physical braking time.
- Continue the range/ownership review for application state and work buffers.
  Compact MQTT alone does not complete Change Requirement 1.
- Later, repeat the same physical A/A/Z test and explicitly cover full A during
  recovery. The first pass already contains Z during ongoing recovery, but
  both A captures were straight with no observed recovery.

The car can remain powered off while these software/evidence tasks are prepared.
