# Hardware wiring

This table records the current assembled robot. Update it before changing any connection.

## Robo Pico connections

| Device | Connection | Pico GPIO | Status |
| --- | --- | --- | --- |
| HC-SR04+ trigger | Grove 1 | GP0 | Tested |
| HC-SR04+ echo | Grove 1 | GP1 | Tested |
| GY-511 SDA | Grove 3 | GP4 | Tested |
| GY-511 SCL | Grove 3 | GP5 | Tested |
| Left motor driver A | M1A | GP8 | Tested |
| Left motor driver B | M1B | GP9 | Tested |
| Right motor driver A | M2A | GP10 | Tested |
| Right motor driver B | M2B | GP11 | Tested |
| SG90 servo signal | Servo header | GP12 | Tested |
| Start/test button | Robo Pico button | GP20 | Tested |
| Secondary test button | Robo Pico button | GP21 | Tested |
| Left IR sensor, AO | Grove 5 | GP26 | Connected; light/dark readings recorded |
| Right IR sensor, AO | Grove 6 | GP27 | Connected; light/dark readings recorded |
| Centre IR sensor, AO | Grove 7 | GP28 | Connected; light/dark readings recorded |
| Left motor encoder A/B | Grove 2 | GP2/GP3 | Firmware ready; wiring pending |
| Right motor encoder A/B | Grove 4 | GP16/GP17 | Firmware ready; wiring pending |

For each IR sensor: red connects to VCC, black to GND, and yellow connects to AO. The white Grove signal and the sensor's DO output are currently unused.

## Motor polarity

The tested integration prototype uses:

```c
LEFT_FORWARD_SIGN = 1.0f
RIGHT_FORWARD_SIGN = -1.0f
```

Motor wire colours are not sufficient evidence of polarity. Confirm direction with the driven wheels raised after any rewiring.

## Mechanical limits

- Servo/sensor assembly demonstrated approximately 45-130 degrees of physical travel.
- Automated ultrasonic scans use 50-125 degrees for clearance.
- Ultrasonic wiring must remain outside the sensor's field of view and servo travel.

## Power

- Robo Pico VIN accepts the project power source within the board's documented range.
- USB is suitable for programming and low-power bench tests.
- Use the intended external supply/power bank for motor operation.
- All connected devices must share the Robo Pico ground.

## Pending encoder wiring

The encoder portions of both six-pin motor connectors are not yet connected. The kit diagram identifies pins 1-4 as ground, A, B, and supply. Comparing that numbered diagram with the user's photographed connector order gives this provisional colour mapping:

| Motor lead | Left motor via Grove 2 breakout | Right motor via Grove 4 breakout |
| --- | --- | --- |
| Blue, encoder ground | GND | GND |
| Red, encoder A | GP2 | GP16 |
| Green, encoder B | GP3 | GP17 |
| White, encoder supply | 3V3, only after compatibility check | 3V3, only after compatibility check |

Yellow and black are the separate motor-power pair already attached to the M1 and M2 screw terminals. Do not insert the encoder leads into those terminals. The female encoder leads do not fit directly into the white Grove sockets: each motor needs a Grove-to-four-wire breakout cable plus four male-to-male jumper wires. This wiring is planned, not yet physically verified. Confirm that the encoder board operates at 3.3 V before connecting white; never feed a possible 5 V encoder output into Pico GPIO. Disconnect all power before wiring. The integration firmware prints `EncL` and `EncR` raw counts for the first hand-rotation test; it does not yet use them for speed or turning.
