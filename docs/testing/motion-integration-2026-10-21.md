# Motion integration: first physical run, 21 October 2026

Firmware banner: `AAI10 MOTION INTEGRATION v1 | Oct 21 2026 18:53:17`.
Raw supplied output is preserved locally in ignored `serial-output/20261021-motion-v1-first-straight.txt`.
Raw SHA256: `7b63c3553fc2180f673bd2b94f40bf192ef0c9e8abd6469457bbe2537b2166b9`.

The operator estimated about 35 cm travel and explicitly described it as an
estimate. No ruler-measured distance or angle was supplied. Do not turn this
observation into a measured calibration value or adjust CPR/wheel diameter
from it.

## Observed in serial output

- USB `f` selected straight 30 cm while stopped; GP20 started the countdown.
- Target 4047 counts per wheel; motion finished with reason NONE, then remained
  stopped with both PWM commands zero.
- Settled left 4272 (error +225), right 4144 (error +97); invalid transitions 0/0.
- Peak sampled straight skew −65 counts. Settled wheel difference 128 counts;
  this is not the same metric as peak sampled skew during motion.
- Near the endpoint, the last controller report still showed about 2013/2094
  counts/s. The right PWM was already zero while the left continued. Overshoot
  exists in encoder space; sampling/coasting and insufficient slowdown are
  candidates, not a demonstrated unique root cause.
- Terrain reported one hump: +8.8° climb, −6.6° descent, duration 998 ms. If this
  was a level-floor run, it is a false detection needing investigation for
  acceleration/vibration artefacts. No flat-floor confirmation was supplied.
- `Line=LOST` did not stop this selected encoder diagnostic: by design it does
  not steer using the IR line. This is not evidence of a line-following failure.

## Calculated, not physically measured

Using the existing 2543 counts/rev and 60 mm wheel model:

- Left equivalent travel: 316.65 mm; right: 307.17 mm.
- Mean wheel-path equivalent: 311.91 mm, about 31.19 cm.
- Left/right excess relative to 4047-count targets: 16.68/7.19 mm.
- Historical Batch 5 straight result 4255/4217 averages 313.99 mm under
  the same model. The new run is 2.08 mm lower in mean count
  equivalent, so this log does not establish a larger integration overshoot
  than the old recorded run. Neither old nor new value proves ground distance.

## Status and next evidence

The selected straight-motion start/complete/stopped path is demonstrated once.
Distance accuracy, repeatability and turn integration remain unverified.
No tuning constants have been changed in response to the estimate.

Next: repeat `f` once with a marked start/end position of the SAME point on the
car and a ruler/tape measurement; preserve the settled line, and report whether
it drove on a level surface and remained straight. This is a targeted check of
new integration/overshoot, not a repeat encoder hand-rotation calibration.
