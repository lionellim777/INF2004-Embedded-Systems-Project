# Encoder inspection v2

This is a component diagnostic for the assembled Robo Pico/Pico W car.
GP8, GP9, GP10 and GP11 are outputs held low; the program never commands
powered movement. USB serial is enabled; UART stdio is disabled.

## Run the check

1. Keep motor power off and raise the wheels. Enter BOOTSEL mode, then copy
   `build/encoder-inspection/encoder_inspection_verbose.uf2` to `RPI-RP2`.
2. Open `build/encoder-inspection/Open Encoder Serial Monitor.command` on this
   Mac. The monitor waits for/reconnects to the Pico at 115200 baud. It sends
   no serial commands and saves output in the ignored `serial-output/` folder.
3. Rotate only the left wheel by hand. Check that both LEFT A/B edge totals
   increase, raw_count changes, and RIGHT stays still. Reverse the rotation
   and check that the sign of the count change reverses. Repeat for RIGHT.
4. Mark a wheel. Press GP20 to zero diagnostic counts. Rotate exactly one
   full revolution, stop, and record the magnitude of raw_count. Repeat three
   times per wheel. Do not force the result to the old 2,543-count reference.
5. Check invalid transitions and any CHECK messages. Record actual results
   before changing calibration constants or proceeding to powered tests.

The firmware prints a help banner whenever USB serial connects and a
timestamped heartbeat plus separate wheel readings every 500 ms. A/B pin
levels are an instantaneous two-bit state, not a full-revolution measurement.
Zero edges cannot distinguish no movement from missing encoder power/wiring.
This test cannot prove powered motor direction, speed, wheel diameter,
floor distance or turn accuracy. Those require subsequent motion tests.

## Build used on 21 October 2026

Pico SDK 2.1.1 (`bddd20f928ce76142793bef434d4f75f4af6e433`), its pinned TinyUSB
submodule, Arm GNU 14.2.Rel1, `PICO_BOARD=pico_w`, Release, `-Wall -Wextra -Werror`.
Build dependencies were installed under `/private/tmp/inf2004-pico-build`.
The serial launcher uses that temporary Python environment; if it is removed,
install pyserial and run `python3 tools/serial_monitor.py` from the repository.

No physical encoder results are claimed by a successful build.
