# v0.5 loaded track check

The installed v0.5 source is `c34205a`. GP20, USB and MQTT moving aborts
have passed raised-wheel checks; the stopped broker restart retained the stop
latch and recovered telemetry without resetting the Pico. See
[GP20 evidence](rtos-v0_5-gp20-stop-acceptance.json) and
[USB/MQTT/reconnection evidence](rtos-v0_5-remote-stop-acceptance.json).
The current Mac broker address and private image hash match the installed
settings. No reflash or password entry is needed for this check.

## Physical steps

1. Use the same full A, half A, then right-bend half Z track. Keep the car
   stopped while preparing. Leave normal motor power connected and initially
   connect USB to the Mac, with the powered wheels raised.
2. Run:

   ```sh
   sh "/Users/cyix/Github/INF2004-Embedded-Systems-Project/tools/run_rtos_network.sh" --reuse
   ```

   Wait for v0.5 USB identity and valid MQTT motion data. Type `t`, then Enter
   to select track mode; the preceding bench tests left `f` selected. Wait
   for `Test selected` before unplugging USB. Selection alone does not move
   the car.
3. Disconnect only the Mac's USB data cable while keeping the Pico powered
   from the car's normal supply. Confirm MQTT data remains available. Place
   the car at the track start: GP28 over centre tape and GP26 over white.
   Hold that position for at least one second.
4. Press and release GP20 to start. Let it pass all three barcodes. After
   GP26 clears the final black stripe, allow another3–5cm onto white if space
   permits, then press GP20 to stop. Use GP20 earlier if the car leaves the
   safe track area. This firmware has no sonar/hand obstacle stop.
5. Keep the Pico powered and the helper open until
   `Scan diagnostics saved; waveform checksum verified.` appears. Wait at
   least five more seconds for the paced task measurements. Type `q`, Enter
   and let the helper finish saving before disconnecting power.
6. Report completion and any observed line loss/stop problem. The agent reads
   the saved local logs directly; no manual file upload is needed. One run is
   enough for the next decision, including if a barcode is missed.

Do not create a deliberate drift for this first loaded check. Natural recovery
is recorded; repeatability and full-A recovery acceptance remain separate.

## Evidence to check after the run

- Real A, A, Z accepted reads and their attempt geometry/recovery intervals.
- Controller/ADC/queue faults, queue high-water mark, failed captures and
  waveform-download checksum. Preserve the first failed capture if present.
- All four run-specific runtime task rows and intact low-stack markers;
  loaded Decoder watermark, armed task entry gaps and elapsed work maxima.
- Armed motor-service age versus40ms, guard spacing and final latched stop.
- Separate on-device timing from MQTT arrival delays. Stack watermarks and
  one-course maxima are observations, not worst-case guarantees.

Terrain, obstacle manoeuvring and barcode-command execution remain with their
owners. The scanner, steering and stack allocations are unchanged for this test.
