# Start the RTOS track build directly on car power

The configured v0.5.1 image has `RTOS_WAIT_FOR_USB=0`. `main.c` selects
`MOTION_COURSE` before starting the kernel. A fresh power-on therefore selects
track mode without a USB `t` command, while leaving motor outputs stopped.
GP20 remains the only start gesture. A new boot is required if the preceding
session selected `f` or another bench mode.

The previous USB-to-car-power handoff rebooted the Pico and lost live telemetry
until USB was reconnected. Retained diagnostics subsequently confirmed A/A/Z
with zero rejects/faults. Starting directly on the intended supply removes
that handoff from the procedure; it is not proof that the underlying wireless
problem is solved. A stationary cold-boot check is the next acceptance item.

1. Stop the car and finish the previous helper with `q`/Enter. Confirm any
   preceding scan download is checksum-verified before switching power off.
2. Disconnect the Mac USB cable. Power the Pico/car off and back on using
   its usual standalone supply. Keep both powered wheels raised for the
   stationary check; do not press GP20.
3. Run the helper on the Mac using the saved settings:

   ```sh
   sh "/Users/cyix/Github/INF2004-Embedded-Systems-Project/tools/run_rtos_network.sh" --reuse
   ```

   The agent may run it during the stationary investigation. No flash, USB
   connection, `t` command or password entry is needed. The Mac stays on the
   same LAN as the Pico and remains running the broker/helper.
4. Wait for **Wi-Fi live**, a new Pico boot ID, idle/stop-latched/fault0 and
   fresh runtime/scan rows. The helper now reports waiting after10s, warns
   once after5s of missing motion data, and prints a live status every10s.
   These are delivery observations, not motor-control timing guarantees.
5. If motion data does not arrive, keep the car stopped. Reconnect Mac USB
   without switching off the car to inspect retained network/scanner state.
   Do not repeat a track run while live logging is unverified.
6. Only after the stationary check passes, place the car on the track with
   GP28 over tape and GP26 over white for one second, then start through GP20.
   After stopping, preserve power until the diagnostic download is complete.

USB disconnection is now described as a switch to Wi-Fi logging, not as waiting
for a flashed Pico. An observed telemetry pause is reported separately from a
proven MQTT reconnection. Commands and firmware are unchanged by this console
update; no automatic or remote START/STOP has been added.
