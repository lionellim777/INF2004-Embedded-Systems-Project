# Buddy 1 integration guide

The public task-context API is in `include/comm.h` and `include/telemetry.h`.
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
