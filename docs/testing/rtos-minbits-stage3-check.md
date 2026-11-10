# Minimum-bit candidate hardware checkpoint

Candidate: full-diagnostic`0.6-dev3`, branch`yixuan/rtos-minimum-bit-state`.
Software evidence is in [the measured report](representation/20261110-rtos-minimum-bit-stage3.json).
No candidate hardware acceptance is recorded yet. The installed/reference image
is still v0.5.1. Use the full diagnostic candidate first, preserving failure
evidence; lean diagnostics are an optional later memory-budget choice.

## First physical action

1. Keep the car stopped and disconnect its motor battery.
2. Support the chassis with both powered wheels clear of the surface.
3. Connect the Pico W's micro-USB port to the Mac normally.
4. Leave BOOTSEL and GP20 alone, and tell the agent that it is connected.

No track is needed for this first check. The agent must first check that the
private image/settings hashes and current broker address agree, preserve the
accepted image, then install/verify the selected candidate by USB. Do not use
the normal setup flow to select this development image: normal provisioning
intentionally retains the accepted flags/version.

## Agent's stationary acceptance

- Confirm exact `0.6-dev3` identity, normal boot, idle/stop-latched/fault0 and
  no drive output. Obtain all four runtime/stack records and scanner status.
- Check USB and MQTT telemetry, STOP acknowledgement and no start on reconnect.
- Keep the current task priorities, stack requests, ADC cadence, stop guard,
  decoder validators and binary schemas unchanged.
- Record image hash, source manifest, boot ID, task markers, timing and logs.
  A version label or successful flash alone is not acceptance.

The agent then gives the next physical instruction. Do not connect the battery
or press GP20 until the stationary result has been checked.

## Subsequent regression, after stationary acceptance

Repeat the existing raised-wheel GP20, USB and MQTT stop checks; confirm the
wheels physically stop and remain stopped after release. Then use the proven
standalone-power wireless startup for the full-A / half-A / bend-half-Z course.
The Mac USB cable is removed for travel; the Mac remains at the broker.
Wait for live Wi-Fi, align the sensors, and use physical GP20 to start.
Retain power until the complete diagnostic capture is saved/checksum-verified.

Compare against v0.5.1: all three target reads, no controller/queue faults,
bounded line recovery, armed Motion/service/guard timing and all low-stack
markers intact. Examine stack watermarks with the compiler frame increases;
do not infer timing or stack safety solely from reduced BSS. Keep final
official-course and teammate integration tests separate from this checkpoint.
