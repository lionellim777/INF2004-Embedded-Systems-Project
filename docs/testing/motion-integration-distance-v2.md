# Measured straight travel and v2 correction — 21 October 2026

## Supplied evidence

The operator measured **340 mm**, using the centre of the wheels as the marker
at both ends of the run. Command: straight 300 mm, using motion integration v1.

- Target 4047/4047 counts; settled 4289/4276; errors +242/+229.
- Invalid transitions 0/0; peak sampled wheel skew 77; final wheel difference 13.
- Motion completed with reason NONE; outputs remained zero after stopping.
- Raw log preserved locally in ignored `serial-output/20261021-motion-v1-measured-340mm.txt`.
- Raw SHA256: `b4e45890a01e1628030ffdfdb2c896c2e18193c5e84901545abaa88e010f0e8b`.
- The log starts with Humps=4/Tilt=38.7 from earlier activity. No new completed
  hump appears during this supplied run. Its existing count must not be
  represented as four false humps caused by this particular run.

This establishes a measured 40 mm / 13.33% overshoot for this run. It does not
establish repeatability, actual wheel diameter, slip, slope, or separate left
and right ground distances.

## Correction chosen

Two observable issues are kept separate:

1. Both wheels exceeded the software target. v1 only checked the target every
   100 ms PI update. v2 checks each wheel's endpoint on every controller poll,
   without changing the calibrated PI update interval or gains. The main loop
   still contains bounded blocking sonar work; this is not an ISR stop guarantee.
2. The old 2543 counts/rev and 60 mm model predicts317.43 mm
   for the settled mean, whereas the measured displacement is 340 mm. v2 uses a
   **provisional straight-only empirical scale** rather than claiming a new
   mechanical CPR or altering the existing turn calibration:

   mean_counts = (4289 + 4276)/ 2 = 4282.5
   counts_per_mm = 4282.5 / 340 =12.595588235
   target_for_300mm =round(300 * counts_per_mm) = 3779 counts per wheel

This model assumes mean wheel travel represents the marked displacement for
this run. It is a one-run estimate, not a validated calibration. It uses total
settled counts to map travel, while endpoint checking separately reduces
sampling overrun. Residual movement after cutoff still needs measurement.
No additional 235.5-count stop offset is subtracted: doing so would mix the old
sampling/coasting error into a second compensation while also changing stop timing.

## New diagnostics and checks

v2 records each wheel's position at its first target cutoff and retains it
through the 500 ms settling interval. Compare cutoff-to-target and settled-to-
cutoff separately. A zero cutoff in a faulted run means the wheel did not
reach its target cutoff; it is not evidence of zero stopping distance.

Host sanitizer tests passed for 3779-count straight targets, unchanged turn
pairs, between-PI-tick stopping, independent wheel cutoff, reverse turn cutoff,
cutoff retention during subsequent movement, abort, invalid transitions,
wrong direction, stalled progress, delayed updates and motion timeout.
Full Pico build and UF2 verification are recorded with the generated artifact.

## Next physical check

Flash the v2 artifact and repeat `f` once, marking the same wheel-centre point
and measuring the 300 mm request. Keep `[MOTION STOP]` and `[MOTION SETTLED]`.
The v2 result is **not yet physically tested**. Do not report successful 30 cm
accuracy until the operator supplies the measurement.
