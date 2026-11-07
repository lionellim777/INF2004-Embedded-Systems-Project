# Remaining project work —7 November2026

Baseline: v5.63. Barcode decoder implementation is complete and physically
validated on the current full-A / half-A / right-bend half-Z setup: two AAZ
runs,6/6 target reads. Preserve its source and UF2 as the regression baseline.
Motor/encoder/IR bring-up does not need restarting without evidence of a fault.

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
| RTOS motion foundation | [Separate real µT-Kernel target](../firmware/app/mission_rtos/README.md) builds with reused motor/encoder control, latched stop guard and13-byte motion snapshot; host safety/codec checks pass | USB bench/loaded tests, measured stop timing and stack margins; then one producer at a time. This target has no Wi-Fi or full mission behaviour yet. |
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
