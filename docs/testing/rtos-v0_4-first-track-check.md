# v0.4 first combined RTOS track check

Historical initial checklist. The first run decoded A/A but missed Z. Use the
[v0.4.1 diagnostic run instructions](rtos-v0_4_1-scan-diagnostics.md) next.

This build combines the unmodified v5.63 centre/right line controller and grey
waveform decoder with real encoder data, binary MQTT and protected motor output.
Terrain/hump and obstacle manoeuvring are not included. Y/Z actions remain
straight so the first comparison uses the same full-A / half-A / right-bend
half-Z course as the accepted v5.63 run. It is not final mission acceptance.

## What is already done

The configured image is local at
`build/rtos-network-v0_4/robocar_rtos_network_v0_4.uf2`. It contains local Wi-Fi
credentials and stays ignored, outside releases/git. The accepted v5.63 and
v0.3 images remain available. The bridge/µT-Kernel dependency is unchanged.
See [the build and test evidence](rtos-v0_4-track-integration.json).

The new target boots without a USB logger and selects track scan by default.
It always starts stopped. A held GP20 at boot and its release do not start it;
a fresh debounced press/release is required. The ADC stays live while stopped.
At start, GP28 needs a stable black-line baseline; rejected placement leaves
the stop latch engaged. The existing controller/scanner algorithms and
80mm-forward/85mm-left GP26 geometry have not been retuned.

## One initial run

1. Prepare the same full A, half A and right-bend half Z test track. Put GP28
   over the central black tape, GP27 to its right on white, and GP26 on white
   before the first barcode. Leave at least a few centimetres of white travel
   before the first bar. Use the normal sensor clearance and mounting.
2. Keep the car stopped. Start the logger on this Mac:

   ```sh
   sh "/Users/cyix/Github/INF2004-Embedded-Systems-Project/tools/run_rtos_network.sh" --reuse
   ```

3. Use the car's normal battery/Pico power supply. USB may remain connected
   only if its cable does not restrict the track. Otherwise use the normal
   untethered power supply: after a normal reboot, v0.4 selects track scan
   automatically and still waits for GP20. Do not use BOOTSEL. If using a
   USB power bank, it can boot directly from that; a computer USB logger is
   unnecessary. Keep the Mac connected to the home router.
4. Wait for valid MQTT motion data and `Pico v0.4 ... online` if USB is attached.
   In untethered operation the MQTT data is the connection check; no USB status
   line is expected. `t`, Enter selects track mode when USB is attached; this
   is useful if another test was selected without rebooting. It never starts
   movement. No remote START command exists.
5. Hold the car still on the start line for at least one second. Press and
   release GP20 once to start. Expect the car to follow the tape, decode A,
   decode A again and then decode Z during the bend. The Terminal prints
   `MQTT barcode: A`, `A`, `Z` with distinct capture times. Local acquisition
   runs during steering/recovery; MQTT carries the latest decoded snapshot.
6. Press GP20 after the half Z and before lifting the car. Release it and
   confirm drive stays off. This build has no sonar/obstacle stop: putting a
   hand in front is not its stop method. `m`, Enter can also request MQTT STOP;
   packet delays mean GP20 remains the immediate physical stop.
7. Once the wheels are stopped, type `q`, Enter in the Mac console. It requests
   stops, waits up to five seconds for a wireless acknowledgement, and saves
   the USB/MQTT capture files. Report the observed letters, whether the bend
   completed and whether drive stayed off after GP20 release. Files are in
   the shared `serial-output/` folder; no manual upload is needed.

If there is a fault or missed barcode, keep this first capture rather than
repeating many runs. USB records include ADC age, ADC/decoder work duration,
queue peak, scanner fault bits and recovery counts. MQTT contains real raw
line samples and motion state. The scan runs for at most 180 seconds and the
reused course controller retains its 160000-count per-wheel bound.

## Acceptance boundary

The host tests exercise the actual ADC callback, compact sample queue, decoder,
course controller and motor gate together, including full/half A, Z during line
recovery, raw-button abort, aborted-frame reset, stale ADC, queue overflow,
decoder-arming timeout, off-line start rejection and clock/count boundaries.
Recorded waveform fixtures also replay through the unmodified decoder.
These checks do not establish physical bend performance or loaded stop timing.

The static image uses a 32KiB kernel pool; application task stack requests total
20KiB inside that pool. Linked RAM is not the same as measured free heap or
runtime stack headroom. Complete mission integration, minimum-bit application
coverage and timing/stack measurements remain open.
