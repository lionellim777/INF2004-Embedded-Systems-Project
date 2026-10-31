# v3 measured 30 cm and end pivot; v4 balancing — 21 October 2026

The operator reported **30 cm with minimal error** using the same wheel-centre
markers, and a slight left pivot at the end. No uncertainty interval or angle
measurement was supplied. Raw log is preserved locally in ignored
`serial-output/20261021-motion-v3-300mm-end-left-pivot.txt`.

| Reading | Left | Right |
| --- | --- | --- |
| Target | 3537 | 3537 |
| Target cutoff | 3543 | 3546 |
| Settled | 3631 | 3627 |
| Additional counts after cutoff | 88 | 81 |
| Invalid transitions | 0 | 0 |

The final wheel difference is only four counts, but the left led by up to 94
counts during travel. The last moving report was 3493/3422, a 71-count left
lead. The likely mechanism is an end catch-up: the leading wheel reaches its
target and its power is cut before the lagging wheel finishes, creating a
brief left pivot. v3 has no cutoff timestamps, so their precise order/delay
was not measured. Post-cutoff rolling is nearly equal; it alone does not
explain a substantial left pivot. Tyre slip or caster/body effects are not
excluded. Do not infer a measured yaw angle from this log.

## v4 change

- Keep the successful 3537-count 30 cm endpoint and all turn targets unchanged.
- Retain calibrated PI gains, speed ramp and 100 ms PI interval.
- For straight travel only, add bounded position feedback after the PI output:
  `balance = clamp((left_position-right_position)*0.0003, -0.04, +0.04)`.
  Subtract it from left PWM and add it to right PWM, then clamp both to 0..0.65.
- The correction can reduce the leading wheel below the old 0.25 PWM floor
  while moving. Previously, both wheels could be at that floor near the end,
  preventing the existing speed-target trim from removing their position gap.
  At zero position gap the added correction is zero. It is applied only on
  PI updates, not accumulated on intervening polls. No new active-braking
  driver mode or fixed left/right endpoint mismatch was introduced.
- Add each wheel's target-cutoff timestamp in microseconds from motion start
  (`cutoff_us=L/R`) to STOP and SETTLED diagnostics. This measures software
  cutoff decisions, not the exact time a physical wheel stopped rotating.

The new gain is an integration trial, not physically qualified. It aims to
reduce the gap before the endpoint so less final catch-up is required. Each
wheel still has its own target cutoff; a remaining gap can still cause a
catch-up pivot. Existing fault/abort paths retain precedence.

## Checks

Host ASan/UBSan tests passed for correction signs in both directions, independent
numerical PI-plus-balance outputs, correction at the old PWM floor, no repeated
accumulation between updates, cutoff timestamps and retention, zero-gap PI,
unchanged turns, encoder wrap, independent completion, abort and fault paths.
Full Pico W build passed. v4 physical behaviour is **pending**.

Next: run `f` once in v4. Report whether the final left movement is gone,
smaller or unchanged, whether straight travel developed any weave, and whether
the measured distance remains near 30 cm. Exact angle measurement is not needed.
Preserve STOP/SETTLED lines to compare cutoff delays and final position gap.

This does not establish general speed tracking, arbitrary-distance odometry,
RTOS deadlines, physical turn accuracy or full minimum-bit compliance.
