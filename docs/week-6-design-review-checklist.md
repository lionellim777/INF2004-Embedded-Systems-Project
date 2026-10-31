# Week 6 design review checklist

Working checklist for the INF2004 autonomous robotic car group. The course's `Week_6_Design_Review_Template.docx` is the submission format; this file is a planning aid, not a substitute for it.

**Due:** Saturday 10 October 2026, 23:59 Singapore time, based on the Week 1 lecturer briefing and SIT's 2026 calendar. Verify the exact LMS submission time and format before upload. The lecturer described this as a required hurdle and warned of a penalty for late submission.

## Source and evidence rules

- [ ] Use the official group project reference pack, especially its mission requirements on page 12, as the requirements baseline.
- [ ] Use the supplied Week 6 template and complete every required section. Replace its toaster-oven examples with robot-specific content.
- [ ] Separate **verified hardware/test facts** from **proposed design** and **unverified assumptions**. A working standalone test does not prove the integrated robot works.
- [ ] Resolve any disagreement between repo notes and recent test evidence. `docs/hardware-wiring.md` still says encoder wiring is pending, while the later encoder tests verified both channels.

## 0. Team role and subsystem ownership

- [ ] Enter group ID, each buddy's name/student ID, and primary subsystem responsibility.
- [ ] Assign one owner for each major device and software function, including final integration and safety decisions. The official brief suggests Buddy 1 Wi-Fi/MQTT, Buddy 2 motion, Buddy 3 IR/barcode, Buddy 4 IMU, and Buddy 5 ultrasonic/obstacle recovery; confirm actual team allocation.
- [ ] Agree on the interface between teammate-owned Wi-Fi work and the robot controller. Do not claim the Wi-Fi module is integrated yet.

## 1. Requirements

- [ ] Write numbered, testable functional requirements for line following, barcode decoding and all four navigation commands, hump detection and highest-peak reporting, obstacle detection/profiling/bypass/line reacquisition, and Wi-Fi telemetry. Include safe start/stop and fault handling where needed.
- [ ] Write measurable non-functional requirements for timing, accuracy, reliability, resource use, and recovery. Mark any proposed numeric target as a design target until the team agrees or validates it.
- [ ] For each requirement, identify the responsible task/module and planned verification method. Preserve the official brief's wording where it is specific; do not invent a physical hump-height capability from the current peak-angle test.
- [ ] Check the selected RTOS/kernel requirement against the current module announcement, then state it consistently in the design. Current integrated firmware is not an RTOS implementation.

## 2. Hardware integration pin layout

- [ ] Draw one readable Robo Pico system sketch showing sensors, actuators, Pico W Wi-Fi, motor/encoder power, common ground, and signal direction.
- [ ] Label the verified pins and interfaces: ultrasonic GP0/1; left encoder GP2/3; IMU I2C GP4/5; motors GP8/9 and GP10/11; servo GP12; right encoder GP16/17; buttons GP20/21; IR ADC GP26/27/28.
- [ ] Check power-supply and logic-voltage notes before finalising the sketch. State which connections are verified and which require confirmation. Clarify that the left/centre/right IR sensor order is Grove 5/7/6, not GPIO numerical order.
- [ ] Record the unresolved physical geometry: the two chassis sensors may straddle the 18 mm line while the offset sensor scans a side barcode. Do not present the current centre-over-black assumption as confirmed.

## 3. RTOS task table and priorities

- [ ] Choose a feasible task set with names used consistently throughout the report. A starting proposal is Safety/Mission Controller, Motion Control, Line/Barcode Sensing, Obstacle Scan, IMU/Terrain, and Communications. Decide whether barcode decoding belongs in the line task or a separate task.
- [ ] For every task, fill purpose, inputs, outputs, periodic/event trigger, preliminary priority, and hardware ownership. Give proposed periods or response deadlines where they matter.
- [ ] Justify relative priorities, especially motor shutdown, motion control, sensor acquisition, scanning delays, and Wi-Fi work. Make explicit that these are Week 6 design choices, not measured RTOS performance.
- [ ] Decide which task alone can command motor PWM and how a safety stop overrides ordinary motion commands.

## 4. Task interaction diagram

- [ ] Draw tasks, hardware devices, interrupts, queues/notifications, and any mutex or event group actually needed. Distinguish an ISR from a task and a sensor from software.
- [ ] Show encoder interrupts to motion control; IR readings and barcode events to mission control; ultrasonic scan results and IMU events to mission control; motor commands to the sole motor owner; telemetry snapshots to communications.
- [ ] Label data direction and ownership. Explain shared I2C/ADC access, queue sizes or overflow policy where relevant, and what happens if Wi-Fi disconnects or a sensor times out.

## 5. System-level statechart

- [ ] Model boot/calibration, stopped/ready, line following, barcode command execution, obstacle stop/scan/bypass/reacquire, and fault/safe stop.
- [ ] Label each transition with an event or condition; include GP20 start/stop, valid obstacle detection, loss of line, timeouts, and recovery.
- [ ] Define what the motors do in every state, particularly at boot, during scan, and on fault. Treat ultrasonic `no echo` as unknown rather than clear.

## 6. At least three subsystem statecharts

- [ ] Choose and name three relevant subsystems, their related RTOS tasks, and why a statechart helps. Good candidates: line/barcode navigation, obstacle scan and recovery, and IMU/hump monitoring or motion-command execution.
- [ ] For each, show normal states, triggering events, timeouts/faults, reset or recovery, and the output/event passed to the mission controller.
- [ ] Check that subsystem events match the system-level statechart and the task interaction diagram.

## 7. Assumptions and validation plans

- [ ] Record at least one assumption per buddy, with risk if wrong, validation method, owner, and status.
- [ ] Include known open issues: actual IR sensor geometry and barcode capture; motor-control integration; ultrasonic limits and obstacle width estimate; IMU peak angle versus physical hump height; Wi-Fi/MQTT interface and reconnect behaviour.
- [ ] Plan safe validation after the physical-work pause. Clearly label tests that are deferred; do not convert assumptions into verified claims.

## 8. AI-supported design evidence

- [ ] Capture genuine pre-submission AI ideation for each buddy: prompt/topic, useful suggestions, weak suggestions, and the team's decision. Do not fabricate conversations.
- [ ] Record the team-reviewed architecture freeze: accepted/modified/deferred ideas, engineering reason, and affected artefacts.
- [ ] Run a final strict AI gap review of the completed design. Log each gap, team response, action, and supporting evidence or reason. Apply changes to the design before freezing it.

## 9. Design consistency and final review

- [ ] Answer the template's consistency questions: device owner; highest-priority task; statechart-to-task mapping; functional-requirement-to-task/state mapping; expected post-Week-6 changes.
- [ ] Cross-check every name, pin, task, queue, state, requirement ID, and owner across all sections. A traceability matrix can make this quick.
- [ ] Remove all toaster-oven examples and placeholders. Ensure diagrams are legible, every required section is filled, and all team members can explain the submitted design.
- [ ] Confirm group ID, file format, upload destination, exact LMS deadline, and submission responsibility. Submit the completed document before the stated deadline.

## Recommended work sequence while physical development is paused

1. **Team decisions:** confirm buddy roster and ownership, RTOS choice, current pin map, and which physical facts remain uncertain.
2. **Design baseline:** write the requirement IDs and verification methods, then freeze task names, ownership, and interfaces.
3. **Diagrams and statecharts:** draw the pin layout and task interaction diagram; draft the system and three subsystem statecharts from the same names/events.
4. **Review:** assign one assumption to each buddy, collect real AI evidence, perform the final gap check, and resolve inconsistencies.
5. **Submission:** fill the supplied Word template, review the exported pages with the team, confirm the LMS details, and upload.

## Current project evidence available for drafting

- `docs/project-requirements.md` gives a mission summary; the official brief remains authoritative.
- `docs/hardware-wiring.md` has the pin mapping, but its encoder-wiring status is stale. The handover and standalone encoder tests provide later evidence.
- `docs/architecture.md` has a proposed vehicle state flow, but it is not an RTOS task design or completed statechart.
- `docs/development-status.md` records what has been tested. Standalone motion, IMU, ultrasonic, and IR tests support design choices; integrated line navigation, obstacle bypass, Wi-Fi/MQTT, and complete mission remain unverified.
- The pending IR calibration patch changes reference values only; it does not resolve physical sensor placement. It has not been applied in this checkout.
