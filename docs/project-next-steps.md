# Remaining project work —8 November2026

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

The next implementation milestone is a lean RTOS mission target that combines
existing line following, the validated v5.63 barcode acquisition/decoder,
real motion snapshots, binary MQTT and protected stop handling. The first
physical acceptance should exercise the known full-A / half-A / bend half-Z
track with networking active. It can be prepared before the terrain/obstacle
owners finish their algorithms; their interfaces and motor ownership must be
agreed before connecting them. Keep the accepted v5.63 build available.

Implementation update, 8 November: v0.4 now links that core on
`yixuan/rtos-track-integration`. The exact v5.63 course/waveform sources are
unchanged. The host ADC-to-PWM pipeline tests and stationary Pico boot/network
check pass; the [first combined moving-track check](testing/rtos-v0_4-first-track-check.md)
is next. Treat the AAZ software simulation as software evidence, not a moving
hardware result. No teammate terrain/obstacle branch has been merged.

Prioritize a first integrated core during Week 11, then add the owners' ready
navigation/terrain/obstacle modules and reserve Week 12 for course regressions,
minimum-bit measurements and assessment evidence. These are planning targets,
not promised delivery dates; Week 12 also contains the individual assessments.
Avoid optional network/UI tuning once it no longer blocks the mission.

Remote branches were refreshed on 8 November. Jinghai has pushed command
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
| RTOS motion and binary MQTT foundation | [Separate µT-Kernel target](../firmware/app/mission_rtos/README.md); [v0.2 USB bench accepted](testing/rtos-v0_2-usb-acceptance.json). [v0.3 user network bench complete](testing/rtos-v0_3-user-network-bench-acceptance.json); [live broker recovery accepted](testing/rtos-v0_3-broker-restart-recovery.json) without a Pico reset or cleared stop latch | Integrate real line/barcode and the other mission producers; confirm physical stops, control timing and stack margins under load. Whole-application packing remains pending. |
| Minimum-bit representation | [Initial target inventory and measured baseline](representation-audit.md):658 direct fields across59 types; actual RAM and synthetic serialized-message sizes | Complete range/lifetime/ownership decisions throughout application data; implement safe packing and binary MQTT; measure after-change RAM/messages and verify functionality/timing. The current Wi-Fi serializer changes Z to `?`. |
| One mission and memory/data integration | Calibration-mode controller, separate barcode and avoidance modes; packed-data work on Buddy branches | One automatically coordinated run for following, decoding/actions, terrain, avoidance/rejoin and telemetry. Barcode acquisition is currently disabled in obstacle mode. Reconcile peripheral ownership, timebases, task/core use and data formats. |
| Final acceptance/demo evidence | Versioned builds, host checks and individual hardware records | Repeated whole-course tests, communications loss/recovery and fault handling; record final firmware, configuration, video/logs and required report/demo evidence. |

Current allocation: hump work is with the terrain member, barcode-command
execution is with another member, and obstacle integration is on hold pending
Lionel's reply. Keep v5.63 barcode logic unchanged. The work available now is
representation/RTOS preparation and the shared communication/motion interfaces;
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
representations, with measured Week13 evidence. The existing JSON interface
does not satisfy that binary-payload requirement. This review checks supplied
course files and fetched code, not the live LMS or teammates' unpushed work.
