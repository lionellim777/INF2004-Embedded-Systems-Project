# First RTOS motion hardware check

Use candidate **v0.2** on branch `yixuan/rtos-motion-foundation`. The software
build, host motor/stop tests, binary codec tests and local MQTT broker tests
have passed. The Pico has not been connected over USB or flashed by the agent.
This check establishes actual RTOS boot, encoder-driven motion and stop/restart
behaviour before adding more producers. It is unrelated to hump sensing.

## What to prepare

- The car with its usual motor power and a USB **data** cable to this Mac.
- A stable support under the chassis so **both powered wheels can turn freely**
  and the car cannot drive away. A box/block is sufficient; no hump or slope.
- The candidate: `build/rtos-motion/robocar_rtos_motion_v0_2.uf2`.
- The normal v5.63 firmware remains at
  `firmware/releases/robocar_motion_integration_v5_63_wireless.uf2` for restoring
  ordinary track operation after this test.

This candidate uses USB for this check. The usual `192.168.4.1` page will not
be available. Barcode capture, obstacle stopping and terrain sensing are not
enabled. **GP20 is the physical stop input.**

## Do this in order

1. Support the chassis with the two powered wheels free. Flash the v0.2 UF2
   using the same BOOTSEL method used for earlier firmware. Leave USB connected
   and ensure normal motor power is connected. Leave GP20 released.
2. Open Terminal and run:

   ```sh
   sh "/Users/cyix/Github/INF2004-Embedded-Systems-Project/tools/run_rtos_bench.sh"
   ```

   It detects the Pico and saves logs automatically. Wait for
   **“RTOS v0.2 confirmed.”** Both wheels must remain undriven at boot. If no
   identity appears, do not select/start a test; keep the captured output.
   The default build waits for the USB logger before starting its RTOS tasks;
   this is expected. A `[RTOS_BOOT]` line records the kernel-start stage.
3. Type **`f`**, then Enter. Selection alone must not move the wheels. Press
   **and release GP20** to start. Once the wheels turn, press GP20 to abort.
   Hold it for about one second, then release. Drive must cut out and remain
   off after release; wheels may coast briefly after PWM is removed. Tell us
   whether drive was removed and whether it restarted without a fresh gesture.
4. Press/release GP20 again to start a second run. While it is turning, type
   **`x`**, then Enter. Confirm drive cuts out and stays off. This tests the USB
   stop path through the same latch.
5. Press/release GP20 once more and let the bounded run finish. The selected
   “30 cm” test turns the wheels through its encoder target while the chassis
   stays supported. It must report **completed** and remove drive. This is
   encoder endpoint evidence, not a measured 30 cm floor-distance result.
6. Type **`q`**, then Enter to finish logging. Confirm the wheels are undriven
   before lowering the car. Send the **`.log` and `.json` files** printed by
   the helper; both are under `serial-output/`. Include your observations from
   steps2–5. If a fault occurs, stop the test and send the files; repeated runs
   are not necessary to diagnose the first failure.

The helper sends only test selection/status/stop bytes after confirming v0.2.
It never sends START. Every run requires the physical GP20 gesture. A USB
disconnect does not itself certify a stop; use GP20 and observe the drive.

## What the logs establish

- Recurring decoded records: Motion and Observer tasks are progressing on the
  actual board; capture time, valid speed, relative counts and state are visible.
- Completed encoder target: actual encoder feedback reaches the bounded
  controller endpoint.
- Stop traces: the timestamp of firmware detection and the subsequent PWM-off
  register writes, plus maximum alarm interval. These exclude the unobserved
  physical press-to-detection interval and mechanical coast/braking time.
- Your observations: whether the wheels were driven at boot, whether the local
  and USB stops removed drive, and whether abort release caused a restart.

This test does not yet establish loaded distance/turn accuracy, a complete
50 ms physical stopping guarantee, stack margins or Wi-Fi timing under load.
Those follow after the first board results. No barcode or hump test is needed
for this first check.

## Later radio test

The binary MQTT adapter and receiver are built, but the default candidate has
empty network settings and does not initialize the radio. After the USB check,
configure the ignored local network settings and validate the Pico against the
team's actual broker: receive real motion, verify unavailable sensor flags,
disconnect/restart the broker, and repeat stop tests while publishing. The
local test broker used by the agent binds only loopback and is not a LAN
broker for the Pico.
