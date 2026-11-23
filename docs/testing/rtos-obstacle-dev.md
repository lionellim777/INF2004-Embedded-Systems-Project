# RTOS obstacle candidate

Build `firmware/app/mission_rtos` with `-DRTOS_OBSTACLE=ON` (or set
`ROBOT_OBSTACLE=ON` for `tools/build_rtos_motion.sh`). The default
selection is the existing track mode. Select `o` over USB while stopped
to choose obstacle mode, then use the normal GP20 press/release to start.
Use `t` to restore track mode. The obstacle selector is local; motion
telemetry uses the existing track record format and status is shown in
`[TRACK_INFO]`.

The mission build joins the SSID in the ignored
`firmware/app/mission_rtos/network_settings.local.h`. Copy the example
header, set `RTOS_NETWORK_CONFIGURED` to 1, and enter the SSID, password and
broker address before building a radio image. With the example settings,
the image is for USB tests and Wi-Fi is disabled.

## Obstacle field ranges

| Fields | Range, units and representation |
| --- | --- |
| Bypass, next and resume phase | 26 enum values, 5 bits each with a compile-time capacity check |
| Per-angle near, far and completed no-return counters | 0–3 each, 2 bits each, one byte per ray for eight rays |
| Scan index, sweep totals and angle | Terminal ray index included, 4 bits. Up to 24 observations per sweep, 5 bits. Servo command 50–125° and measured ray angles, 7 bits |
| Departure and return angles, step distance | Angle up to 90°, 7 bits. Step up to 100 mm, 7 bits |
| Accepted lateral, diagonal and along distances | Lateral cap 600 mm and diagonal below 850 mm, 10 bits. Along limit 1,200 mm plus a final leg, 11 bits |
| Axle advance and alignment | Validated 40–140 mm before storing, 8 bits. Direction −1, 0 or +1, signed 2 bits. Reversal and arming are single-bit flags |
| Encoder origins | Modulo-32 raw counts, one 32-bit count per wheel. The run origin also stores the initial invalid-transition counters. Active and paused motion legs retain the complete controller they need |
| Echo pulse and range | Pulse width in microseconds, bounded to 45,000, stored in 16 bits before conversion. Valid range 2–400 cm using 0.01715 cm per microsecond. Planner ranges and computed estimates retain float precision |
| Timers, diagnostic totals, computed estimates and fault reasons | Current widths retained. A narrower form needs an explicit uptime or saturation contract, precision budget, or stable reason-code mapping |

The obstacle controller occupies 1,936 bytes in the Pico W RTOS image.
The original wide state measured 2,952 bytes under the same compiler
layout, a saving of 1,016 bytes. These sizes exclude the sonar adapter
and describe the obstacle controller rather than the complete image.

## Hardware ownership and acceptance

GP0 generates a 10 µs trigger using PIO. GP1 echo edges are captured by
a raw GPIO IRQ handler alongside the existing encoder handler. Motion
polls the capture without sleeping, distinguishes no rise, completed no
return, valid range and faults, and drops a pending capture when the
servo angle changes. GP12 uses its own PWM slice for the servo. The
fixed timing values are a 60 ms minimum trigger interval, 30 ms rise
deadline and 45 ms high-pulse deadline. The control loop keeps the
existing two sweeps, 100 mm step and left-first tape alignment.

Motion remains the sole owner of obstacle state. Its motor proposals
pass through the existing board gate and run token. A latched stop
zeros the motor PWM, and the next Motion service disarms echo capture
and centres the servo. Existing ADC and barcode acquisition continue.

The obstacle host regression passed for detection, detour and tape
rejoin. The RTOS host test with simulated sonar also passed detection,
left-turn output, cancellation and stale-token rejection. Windows
Application Control blocked the separately compiled wide-state trace
comparison. The sonar IRQ and servo timing still require a car test.

Before hardware acceptance, check GP20 STOP with the wheels raised and
while the sonar is waiting for an echo. Then check a supervised floor
run for detection, left departure, 100 mm progress, right restoration,
straight tape seeking and left-first rejoin. Record stop latency,
Motion service gaps, and any sonar fault or repeat. Repeat the A/A/Z
track run separately with `t` selected.
