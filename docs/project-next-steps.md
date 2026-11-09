# Remaining project work —9 November2026

Current accepted RTOS core: v0.5.1, implementation`924eefe`, branch
`yixuan/rtos-barcode-recross`. [Two physical A/A/Z runs now have6/6 target
reads](testing/rtos-v0_5_1-wireless-track-acceptance.json); the latest has all
three real events received live over Wi-Fi with no Mac USB connection. Full A
was accepted while the observed recovery counter rose0→1; bend Z14→20.
ADC/queue/controller fault bits are zero, all four task low-stack markers are
intact, and armed motor-service age peaked at10445us against the40ms limit.
This establishes the current tested core, not the complete mission or all-track
reliability. The first run's USB-handoff failure remains recorded; direct
standalone-power startup is the demonstrated procedure. A5.93s telemetry pause
in the latest session occurred while idle before motion; transport is not a
hard-real-time channel.

No further USB connection or track run is needed for the next software work.
The [incremental minimum-bit conversion](minimum-bit-conversion.md) has started
on `yixuan/rtos-minimum-bit-state`. Stage1 is an opt-in compact ADC/filter
candidate with output equivalence tests; the installed v0.5.1 image and default
build remain the reference. Do not defer representation review until every
teammate algorithm is finished. Agree their data contracts now, and review
each implementation as it arrives. Tribunal1 proposals are checked/corrected
in the implementation record; the submitted report is preserved.
The [current ARM layouts/linker/payload evidence](testing/representation/20261109-rtos-v0_5_1-after.json)
has been refreshed. Next: finish application-owned range/lifetime/access
decisions, prepare a lean diagnostic-memory profile without changing the
accepted scanner, and finalize module adapters when owners provide their
interfaces. Preserve this diagnostic source/image while making a lean profile.
Then integrate ready teammate modules one at a time and perform full-mission
acceptance. No project-completion percentage is inferred from these subsystem
passes.

Baseline: v5.63. Barcode decoder implementation is complete and physically
validated on the current full-A / half-A / right-bend half-Z setup: two AAZ
runs,6/6 target reads. Preserve its source and UF2 as the regression baseline.
Motor/encoder/IR bring-up does not need restarting without evidence of a fault.

## Verified assessment window and immediate priority

Checked the downloaded school materials on 8 November 2026. The Week 0/1
lecture PDF, pages 8-9 and 12, lists the Week 10 subsystem check-in as a hurdle,
Tribunal 2 in the Week 13 lecture slot, and the final demo/code/test review in
the Week 13 lab sessions. `Change Requirement 1.docx` requires the application
representation table, RAM/message comparisons and correctness/timing evidence
in the repository for Week 13. These are course requirements, not instructions
to modify the student's completed Tribunal report.

The Tribunal 1 brief transcript identifies 19 October for the Week 8 lecture
and pre-tribunal deadline, and 23 October for the post-tribunal deadline.
Counting calendar weeks gives Week 11 as 9-15 November, Week 12 as
16-22 November and Week 13 as 23-29 November. This is an inference from the
downloaded schedule, not a verified final timetable. The exact team lab slot,
final upload cutoff and any later LMS changes have not been checked. Confirm
that the Week 10 hurdle has already been completed; completion is not evidenced
by a functioning robot alone.

The combined RTOS core is now implemented on `yixuan/rtos-track-integration`.
RTOS v0.4.2 at `916c0dc` has [one verified physical A/A/Z pass](testing/rtos-v0_4_2-track-acceptance.json):
full A at 42.872s, half A at 52.858s and bend Z at 65.448s, with no scanner or
controller fault. The waveform download is complete and checksum-verified.
During the Z capture, the recovery counter rose from 11 to 16 in observed
states. Both A captures had no observed line recovery. This establishes one
combined line/barcode/motion/MQTT run, not repeatability or full-A recovery
acceptance. The Z read used the existing geometry path; the new short-black
path is separately verified by replaying the prior failed capture.

The next software work is runtime timing/stack/stop evidence and the remaining
application representation review. [Current ARM/linker/payload measurements](testing/representation/20261109-rtos-v0_4_2-after.json)
are recorded: BSS205868, a same-semantics sample queue reduction of1024bytes,
and real binary A–Z transport. The temporary first-failure cache consumes32KiB
plus metadata; remove or reduce it after repeatability/full-A recovery checks.
Static SRAM remainder is not measured runtime heap or stack headroom. Network
arrival gaps reached2.22s in this pass, so delivery timing and local controller
deadlines must be measured separately.

Implementation update: the [v0.5 runtime measurement candidate](testing/rtos-v0_5-runtime-evidence.md)
is now built and software-tested on `yixuan/rtos-runtime-evidence`. Task stack
observations, armed/idle timing, stop sources and USB/MQTT records are implemented.
The first hardware check found MQTT request-slot starvation in a measurement
burst. The paced fix at `c34205a` now sends all four tasks over USB and MQTT.
[v0.5 stationary observations are verified](testing/rtos-v0_5-stationary-acceptance.json):
all low-stack markers intact, controller stopped/no fault, and idle STOP
acknowledged. Static BSS206308 adds440bytes over v0.4.2 with unchanged stack
requests/pool. The measured idle Motion gap includes a138.6ms startup interval.
The [first raised-wheel GP20 abort now passes](testing/rtos-v0_5-gp20-stop-acceptance.json).
The user reports prompt stopping; USB/MQTT agree on an aborted, latched,
fault-free state. Armed Motion entry gap peaked at10023us, service age10140us,
and guard spacing5011us. Both PWM-off writes followed the guard observation
by6us; this excludes button-edge detection and mechanical braking. All four
stack markers remained intact. [Moving USB/MQTT aborts and stopped broker
reconnection now also pass](testing/rtos-v0_5-remote-stop-acceptance.json):
both runs aborted/latched with fault0, Motion entry gaps peaked at10025/10048us,
service ages10140/10177us, and all four low-stack markers stayed intact. The
broker recovered without resetting the Pico or clearing the stop latch.
The [first loaded v0.5 pass](testing/rtos-v0_5-loaded-track-observations.json)
decoded half A and bend Z but rejected full A during recovery. It established
Decoder watermark1408B with intact low markers, armed Motion gap10079us and
motor-service age10426us. The failed full-A waveform is retained and its
first-crossing spatial reconstruction fault is reproduced. The
[v0.5.1 latest-crossing candidate](testing/rtos-v0_5_1-recross-fix.md) now decodes
that saved input in batch/streaming replay; older bend inputs and negative
regressions also pass. [v0.5.1 is installed and stationary checks pass](testing/rtos-v0_5_1-stationary-acceptance.json),
with idle/latched/fault0, all four stack records and idle STOP acknowledgement.
The [first v0.5.1 physical A/A/Z pass](testing/rtos-v0_5_1-track-acceptance.json)
now has three accepted attempts, no rejected captures and no scanner faults.
Full A includes9deg yaw/11mm lateral travel; Z19deg/62mm. However, live telemetry
stopped at the USB power handoff and the results were recovered only after
USB reconnection. The new boot ID establishes a handoff restart; its precise
wireless failure mechanism remains unresolved. A
[fresh standalone-power wireless boot now passes](testing/rtos-v0_5_1-wireless-cold-start-acceptance.json)
while stopped, with no Mac USB connection, fresh data, fault0, the stop latch
set and all four task stack observations intact. Use the
[direct-power startup procedure](testing/rtos-wireless-start.md), avoiding the
unnecessary USB selection/handoff. Next use the already-live session for one
wireless A/A/Z run. That [run now passes](testing/rtos-v0_5_1-wireless-track-acceptance.json),
including the first directly observed full-A recovery increment during capture.
The helper prints live status and missing-data warnings. Armed network-loss timing and repeated whole-course
acceptance remain open. The v0.4.2 rollback image is preserved.

Use the [module handoff draft](rtos-module-handoff.md) to prepare adapters while
members finish their portions. It is a proposal, not a team agreement. No
teammate module is merged, and no changes are needed to their algorithms now.
Keep the accepted v5.63 release and first-passing v0.4.2 image/source intact.
The car can remain powered off during this preparation. Later, repeat the
unchanged A/A/Z build and explicitly cover full A during recovery.

The first integrated core is demonstrated. Next add the owners' ready
navigation/terrain/obstacle modules and reserve Week 12 for course regressions,
minimum-bit measurements and assessment evidence. These are planning targets,
not promised delivery dates; Week 12 also contains the individual assessments.
Avoid optional network/UI tuning once it no longer blocks the mission.

Remote branches were refreshed on9 November; the relevant heads are unchanged
from the previous review. Jinghai has pushed command
configuration work at `b471ee2` (7 November). Lionel's latest pushed obstacle
commit remains `6b60479` (4 November), and terrain remains `4e9ba35`
(26 October). Those dates do not establish the state of unpushed teammate work.
Review useful changes selectively rather than replacing the accepted scanner
with an entire divergent branch. No teammate branch was merged in this review.

This list concerns the integrated main firmware. It does not imply teammates
have not implemented the outstanding functions on their own branches. Official
mission/Buddy requirements were checked against the user-supplied
`INF2004_Group_Project_Reference_Pack.pdf`, mission and Buddy sections.

| Work | What already exists | Remaining acceptance |
| --- | --- | --- |
| Obstacle bypass/rejoin | Scans, bounded bypass, alignment/rejoin and host tests; earlier home successes plus later failures | Repeat straight and bend obstacles without contact, profile/choose route, regain original tape and continue. A sonar stop alone is insufficient. |
| Barcode-driven navigation/junctions | A-Z decoder, encoder turns, configurable Y/Z actions; standalone calibration | Agree final command mapping and execute left/right/straight/U-turns and required junction choices on the actual course. Current AAZ acceptance used Straight actions; A events were unmapped. |
| Hump/peak measurement | Main counts humps and highest pitch; `origin/buddy4-terrain` adds height-validity/data/estimation support | Integrate appropriate height/motion producer, validate actual humps and highest peak in the required units while driving. Pitch angle is not physical height. |
| MQTT and planned µT-Kernel integration | `origin/wifi-buddy-1` contains task/bridge API, telemetry schema and recorded standalone MQTT/heartbeat/command/reconnect observations | Connect real robot producers, preserve motor/sensor deadlines during transmission/reconnect, and verify the combined RTOS robot on hardware. Do not reimplement existing Buddy1 work blindly. |
| RTOS motion and binary MQTT foundation | [v0.5.1 core and live wireless A/A/Z accepted](testing/rtos-v0_5_1-wireless-track-acceptance.json), plus GP20/USB/MQTT moving aborts and stopped broker recovery; loaded task stack and motor-service observations saved | Add remaining real producers through reviewed adapters. Extend network-loss/fault and official-course tests; task watermarks are not worst-case guarantees. Whole-application packing remains pending. |
| Minimum-bit representation | [Initial target inventory and measured baseline](representation-audit.md):658 direct fields across59 types; actual RAM and synthetic serialized-message sizes | Complete range/lifetime/ownership decisions throughout application data; implement safe packing and binary MQTT; measure after-change RAM/messages and verify functionality/timing. The legacy Buddy1 JSON serializer changes Z to `?`; the current RTOS binary codec carries A–Z correctly. |
| One mission and memory/data integration | Calibration-mode controller, separate barcode and avoidance modes; packed-data work on Buddy branches | One automatically coordinated run for following, decoding/actions, terrain, avoidance/rejoin and telemetry. Barcode acquisition is currently disabled in obstacle mode. Reconcile peripheral ownership, timebases, task/core use and data formats. |
| Final acceptance/demo evidence | Versioned builds, host checks and individual hardware records | Repeated whole-course tests, communications loss/recovery and fault handling; record final firmware, configuration, video/logs and required report/demo evidence. |

Current allocation: hump work is with the terrain member, barcode-command
execution is with another member, and obstacle integration is on hold pending
Lionel's reply. Preserve the accepted v5.63 release and the passing v0.4.2 core. The work available now is
representation coverage, a lean diagnostic profile and the shared communication/motion interfaces;
final physical mission acceptance waits for the real producers. Follow the
[audit and integration sequence](representation-audit.md#next-implementation-sequence).

## Integration constraints worth planning now

The v5.63 linked BSS is223436bytes (toolchain size report). Buddy1 documents a
fixed96KiB kernel pool. Naively adding those is321740bytes before other changes,
above RP2040's264KiB total RAM. This is a budget warning from the two current
builds, not a prediction of the final linked image: replace/remove calibration-
only HTTP/log/frozen-waveform buffers and reconcile shared services. Retain the
validated decoder algorithm while changing how optional diagnostic data is
stored. Measure final linker/stack use and control latency on the combined build.

Buddy1's recorded MQTT validation uses simulated mission/motion/line/terrain
producers, with simulated flags documented. Replace them with real producers;
do not present simulated telemetry as integrated hardware validation. The
network/kernel branch and current calibration baseline use different task/core
arrangements, so merging files alone is not integration.

The supplied Week1 lecture transcript explicitly requires µT-Kernel on Pico.
Whether the newer supplied SMP port/configuration is mandatory still needs
confirmation against the current module instructions. Change Requirement1
explicitly requires compact binary MQTT and application-wide minimum-bit
representations, with measured Week13 evidence. The legacy JSON interface
does not satisfy that binary-payload requirement; the separate RTOS target now
implements binary transport, with whole-application minimum-bit coverage still open. This review checks supplied
course files and fetched code, not the live LMS or teammates' unpushed work.
