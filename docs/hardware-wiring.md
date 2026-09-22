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
| IR sensor on Grove 5, AO | Grove 5 | GP26 | Connected; calibration pending |
| IR sensor on Grove 6, AO | Grove 6 | GP27 | Connected; calibration pending |
| IR sensor on Grove 7, AO | Grove 7 | GP28 | Connected; calibration pending |

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

The encoder portions of both six-pin motor connectors are not yet integrated. Before wiring, identify and verify:

1. Encoder ground
2. Encoder output A
3. Encoder output B
4. Encoder supply

Do not infer these four wires from colour alone. Record their final GPIO assignment here after verification.

