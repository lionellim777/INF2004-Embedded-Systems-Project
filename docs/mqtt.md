# Buddy 1 MQTT contract

The robot publishes only over a trusted private 2.4 GHz WiFi network. The
broker is Mosquitto on the laptop, using port 1883, a password, and topic ACLs.
The laptop and Pico W must be able to reach each other directly. Copy
`config/comm_secrets.example.h` to ignored `config/comm_secrets.h` and set the
WiFi SSID, password, laptop LAN IPv4 address, broker user/password, team ID,
and robot ID. Do not commit the populated secrets file.

The topic prefix is `inf2004/<team_id>/<robot_id>/`. IDs contain only lowercase
letters, digits, `_`, or `-`. Replace `team1` and `robot1` below with the chosen
IDs.

| Topic suffix | Direction | QoS | Retained | Content |
| --- | --- | ---: | --- | --- |
| `telemetry` | Robot to broker | 0 | No | Latest snapshot |
| `event` | Robot to broker | 1 | No | Barcode, hump, or scan event |
| `status` | Robot to broker | 1 | Yes | Online heartbeat or `offline` will |
| `command/get_status` | Broker to robot | 1 | No | Empty payload |
| `command/set_period_ms` | Broker to robot | 1 | No | Decimal 200–5000 |
| `command_result` | Robot to broker | 1 | No | Accepted/rejected result |

The robot uses a clean session and resubscribes after reconnecting. Commands
are idempotent. Never retain command publications. MQTT's QoS 1 permits
duplicates; identify distinct events by `boot_id` and `sequence`.

## Payloads

Telemetry is JSON, at most 1024 bytes. This example has simulated producers;
`null` means a buddy has not supplied a valid sample:

```json
{"schema":1,"boot_id":123,"sequence":7,"captured_ms":2000,"mission":{"captured_ms":2000,"simulated":true,"state":"idle"},"motion":{"captured_ms":2000,"simulated":true,"left_mm_s":0,"right_mm_s":0,"left_ticks":0,"right_ticks":0,"distance_mm":0},"line":null,"terrain":null,"obstacle":null}
```

All times are milliseconds since boot. Speeds are millimetres/second,
distances and hump estimates are millimetres, line readings are raw sensor
values, and encoder counts are ticks. Each valid producer record has its own
capture time. Status includes `state`, `boot_id`, `uptime_ms`, `reconnects`,
`dropped`, and `task_ticks`. An unexpected disconnect causes the broker to
publish the retained plain text `offline` Last Will.

Events use `kind` (`barcode`, `hump`, `scan_point`, or `obstacle`), `scan_id`,
`value`, and `angle_mdeg`. A scan point's value is distance in millimetres;
its angle is millidegrees. A hump event's value is millimetres. A barcode
event's value is the ASCII code for `A` through `D`. An obstacle event's value
is the closest measured distance in millimetres. Events expire after five
seconds while the network is unavailable. The oldest queued event remains
queued until publication succeeds; a full 16-event queue rejects a new event
and increments `dropped`.

Diagnostic command examples:

```powershell
mosquitto_pub -h 192.168.1.10 -u observer -P <password> `
  -t inf2004/team1/robot1/command/get_status -n -q 1
mosquitto_pub -h 192.168.1.10 -u observer -P <password> `
  -t inf2004/team1/robot1/command/set_period_ms -m 500 -q 1
```

The response is `{"accepted":true,"command":"set_period_ms",` followed
by `"period_ms":500}`. Invalid commands return `accepted:false`. Commands
do not steer the robot.

## Laptop broker setup

Install Mosquitto and its `mosquitto_pub` and `mosquitto_sub` clients. Create
passwords for `buddy1` and `observer` with `mosquitto_passwd`. Copy the example
broker and ACL files to local paths, replace the IP address and file paths,
then start Mosquitto with that configuration. Bind its listener to the
laptop's private LAN address, allow port 1883 on the private firewall profile,
and test that the Pico W can reach it. Do not use `localhost` as the robot's
broker address. Check access point client isolation if the connection fails.

Subscribe before powering the board:

```powershell
mosquitto_sub -h 192.168.1.10 -u observer -P <password> `
  -t 'inf2004/team1/robot1/#' -v
```

The expected order is an online status, then telemetry every 200 ms and
status every second. Publish the two commands above and watch `command_result`.
Stop the broker or disconnect WiFi, restore it, and confirm that the robot
resubscribes and telemetry resumes. Record an actual timestamped transcript
for the hardware demonstration; this document's examples are not test evidence.
