# Buddy 1 integration guide

The public task-context API is in `include/comm.h` and `include/telemetry.h`.
`port/mtk_bridge.h` wraps task and mutex calls because the upstream BSP and
Pico SDK headers define conflicting C names; application files include only
the bridge and Pico SDK headers. The bridge is the only project source that
includes the micro T-Kernel API header.
`comm_init` copies and checks the configuration. `comm_task` is the only task
that calls CYW43, lwIP, and MQTT. Other tasks publish typed samples through
`comm_update_telemetry` and events through `comm_post_event`. Both copy the
input and attempt a non-waiting mutex lock. If the result is `COMM_BUSY`, retry
on the next producer cycle. If an event returns `COMM_FULL`, count or report
the lost event; do not block motor control.

The app's demo task publishes simulated mission, motion, line, and terrain
samples. Every valid sample in MQTT carries `simulated:true`. Replace one
producer at a time and set `b_is_simulated` to false when its hardware data
is real. Set `b_is_valid` to false until the producer has a meaningful first
measurement. The serializer emits `null` for invalid producers. An obstacle
profile remains unavailable in the demo.

| Owner | `telemetry_source_t` | Fields and units |
| --- | --- | --- |
| Mission controller | `MISSION` | State and capture time (ms) |
| Buddy 2 | `MOTION` | Speeds (mm/s), encoders (ticks), distance (mm) |
| Buddy 3 | `LINE` | Three raw IR values, line flag, barcode |
| Buddy 4 | `TERRAIN` | Motion state, current and peak hump (mm) |
| Buddy 5 | `OBSTACLE` | Scan ID and distances (mm) |

Only Buddy 1 serializes and transmits samples. Other buddies own sensor
reading, calibration, and estimates. No other task may call MQTT or the CYW43
driver. Pass scan points as `TELEMETRY_EVENT_SCAN_POINT` with `scan_id`,
`angle_mdeg`, and distance in `value`; post a final obstacle event and update
the latest obstacle profile after the scan.

To add a producer, populate `telemetry_sample_t`, set `source` to the owner,
fill the matching union field, and call `comm_update_telemetry`. All numeric
fields are copied by value. `captured_ms` comes from the Pico monotonic clock;
do not use wall time. The event queue stores up to 16 copied records and
expires any record more than five seconds old before transmission.

The diagnostic subscriber does not invoke motor or navigation APIs. It only
requests an immediate status or changes the telemetry interval within the
documented range. Integrate the full robot by checking that navigation
deadlines still hold while MQTT publishes at 200 ms and while the laptop
broker repeatedly disconnects.

## Retained motion history

Motion updates through `comm_update_telemetry` now refresh the current motion
snapshot and maintain separate left/right speed histories under the same
non-waiting mutex. Recording does not depend on the network connection.
Each history keeps at most 26 records, replacing the oldest when full. A record
uses four bytes: ten bits for signed speed (-500 to +500 mm/s, two's complement),
21 bits for capture milliseconds since boot, and one reserved zero bit.
Capture times must arrive in order. Buddy 2's agreed producer interval is 200 ms;
the demo producer still uses its existing 500 ms interval.

Repeated equal valid speeds refresh the live snapshot, including capture time,
encoders and distance, without adding history records. A sample with
`b_is_valid=false` refreshes the snapshot as invalid and preserves prior history.
Its numeric speeds are not logged. The next valid sample is retained for both
wheels even if the values equal their last valid records, then normal duplicate
suppression resumes. Recovery uses one bit in each history's existing
bookkeeping; it does not store the start time of the invalid period. The current
motion-validity flag applies to both wheels.

Valid-marked samples must have both speeds in range and capture time within
0..1,800,000 ms (the supported thirty-minute session). Otherwise the update returns
`COMM_INVALID` and changes neither the current snapshot nor either history.
Explicit invalid samples are accepted with `COMM_OK`; consumers must inspect
validity before using their numeric fields. No automatic clock reset or motor
stop is introduced at the session limit.

Use `comm_get_motion_history(&motion, &left, &right)` from task context to copy
current motion and both histories into separate caller-owned objects under one
lock. The call returns `COMM_BUSY` without changing outputs if the lock is busy,
or `COMM_INVALID` for null/aliased history outputs or before initialization.
On success, use `telemetry_wheel_history_count` and `telemetry_wheel_history_get`
on those local copies to read oldest first without removing records. Decode or
transmit after the copy; do not hold the mutex for network activity.

At five updates per second, 26 records cover five seconds including an initial
sample. This corresponds to 250 mm at a sustained 50 mm/s; braking, slower travel,
measurement jitter and invalid/recovery transitions affect distance coverage.
The two histories occupy 216 bytes including bookkeeping/padding on the tested
host ABI, of which 208 bytes hold records. Target RAM usage and lock timing need
hardware verification. This step does not transmit the history over MQTT.
