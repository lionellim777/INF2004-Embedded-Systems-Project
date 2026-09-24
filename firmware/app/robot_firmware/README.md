# Robot firmware: line following, terrain, range, and wheel encoders

## Wheel encoder wiring

Use the six-pin **motor connector diagram supplied with the kit**, not wire
colours. Its pin 1 is encoder GND, 2 is encoder A, 3 is encoder B, 4 is encoder
power, and 5/6 are the two motor wires already connected to M1A/M1B or
M2A/M2B. Do not move motor pins 5/6 to GPIO or 3V3.

The user's photo and stated plug order (outermost to innermost: yellow, black,
white, green, red, blue) resolve the colours against the supplied numbered
six-pin diagram. In that diagram pins 5/6 are the yellow/black motor pair, so
the encoder leads are pin 1 **blue = GND**, pin 2 **red = A**, pin 3
**green = B**, and pin 4 **white = encoder power**. This assignment is inferred
from the supplied plug order, not from a motor manufacturer colour convention.

With the Robo Pico's green VIN screw terminal at the top, Grove 2 is the first
of the five white sockets along the bottom edge, and Grove 4 is the middle
(third) socket. Use the pin labels printed on the Robo Pico, not assumed cable
colours or left-to-right wire order:

| Motor connector pin | Left motor: Grove 2 | Right motor: Grove 4 |
| --- | --- | --- |
| 1, blue, encoder GND | GND | GND |
| 2, red, encoder A | GP2 | GP16 |
| 3, green, encoder B | GP3 | GP17 |
| 4, white, encoder power | 3V3 | 3V3 |

The connector diagram does not specify the encoder supply-voltage range. 3V3
is the safe starting choice for Pico GPIO, but confirm the motors' encoder
board supports 3.3 V before connecting pin 4. Never feed an unverified 5 V
encoder output directly to a Pico GPIO. Unplug all power before wiring.

After flashing, leave the car stopped and turn each wheel slowly by hand.
`EncL` should change only for the left wheel, and `EncR` only for the right.
These are raw quadrature counts; the sign is not calibrated yet. Counts are
reported but not yet used to control speed or turns.

## Line following and other functions

This firmware drives the assembled robot along a black line on the light
styrofoam track. It uses the actual left-to-right sensor placement measured on
23 September 2026:

| Physical position | Robo Pico port | Signal pin | Light | Dark |
| --- | --- | --- | ---: | ---: |
| Left | Grove 5 | GP26 | 191 | 3164 |
| Centre | Grove 7 | GP28 | 178 | 2908 |
| Right | Grove 6 | GP27 | 141 | 842 |

Place the centre sensor over the black line and press GP20 to start. Press
GP20 again to stop. At startup, the wheels remain stopped if all three sensors
see white because the robot has no last known line direction.
With black under the centre sensor and white under the outer sensors, it drives
straight. More black under the left sensor slows the left wheel to turn left;
more black under the right sensor slows the right wheel to turn right.

If the line disappears after being seen, the car continues straight for up to
120 ms to cross a small gap. It then turns in place toward the last side that
saw black. If the line is not found within 900 ms of losing it, the motors stop
and GP20 must be pressed again after repositioning. This is local line
recovery, not obstacle bypass.

The servo is held at its centre position. With the ultrasonic sensor on Grove
1, two consecutive valid readings below 30 cm stop and disarm the motors.
Move the obstacle away and press GP20 to restart. `Range=NO_ECHO` means the
sensor did not return a usable distance; it does not mean the way is clear.
This is an emergency stop, **not** obstacle bypass. The three IR sensors also
capture full-width Code 39 bars using a two-of-three majority. Valid A-D
symbols generate left, right, straight or U-turn requests; duplicate symbols
are suppressed for three seconds. Straight continues, while turn requests
stop safely until Buddy 2's encoder-motion API is connected. Barcode timing
still requires physical validation. Encoder-based turns, obstacle bypass, and
WiFi telemetry are not implemented.

The GY-511 accelerometer now also calibrates for about two seconds at startup,
while the motors are stopped. Place the car level and still before powering it.
Press GP21 while stopped to repeat this calibration. Completed hump events and
the highest climb angle appear in the serial output. The climb angle is a
terrain indicator; it is not yet a physical hump height measurement.

Open the USB serial monitor to see `Run`, `State`, `Line`, the three sensor
readings, distance, hump count and highest climb angle.
The robot starts stopped on every power-up or reset.

The initial 23% motor command was too weak to start the loaded car on the
styrofoam. This build commands 55% for straight travel and reports both motor
commands as `Drive=left/right` in the serial output. If `Run=1 Line=SEEN` and
both commands are above zero but the car still does not move, check its power
source and whether the wheels or front caster are binding.
