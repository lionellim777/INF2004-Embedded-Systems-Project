# Two v2 runs and straight endpoint v3 — 21 October 2026

The operator reported 320 mm travel using wheel-centre markers and supplied a
log containing two completed v2 runs. A separate physical distance for each run
was not supplied; do not claim two independently measured 320 mm distances.
Raw output is preserved locally in ignored
`serial-output/20261021-motion-v2-two-runs-320mm.txt`.

| Run | Target L/R | Cutoff L/R | Settled L/R | Movement after cutoff L/R | Invalid L/R |
| --- | --- | --- | --- | --- | --- |
| 1 | 3779/3779 | 3784/3779 | 3865/3878 | 81/99 | 0/0 |
| 2 | 3779/3779 | 3787/3784 | 3869/3870 | 82/86 | 0/0 |

Both completed with reason NONE and remained stopped. The means are 3871.5 and
3869.5 counts, just two counts apart. Peak sampled skews were 40 and 57 counts;
those are not the same as final wheel differences. Actual ground repeatability
is not established by encoder agreement alone.

v2's between-update endpoint checks are working: cutoff exceeds target by
0..8 counts. Residual movement of 81..99 counts occurs after that decision.
With the representative mean 3870.5 and reported 320 mm, that residual is
approximately 6.7..8.2 mm. Not all of the remaining 20 mm ground overshoot can
be attributed to cutoff latency or rolling. The general wheel-scale model and
measurement uncertainty remain open.

## v3 correction

For the same 30 cm manoeuvre/profile only:

```text
representative settled mean = (3865+3878+3869+3870)/4 = 3870.5 counts
provisional counts/mm       = 3870.5/320            = 12.0953125
20 mm reduction             = round(20*12.0953125)  = 242 counts
new endpoint                = 3779-242             = 3537 counts
```

The prior average target-to-settled excess was 91.5 counts. If it stayed the
same, predicted settled travel would be (3537+91.5)/12.0953125 = 299.99 mm.
That is a prediction for a trial, not a measured v3 outcome.

v3 now stores a named **30 cm endpoint** instead of exposing a general ground
counts/mm constant derived from one run. It does not change CPR, wheel diameter,
turn corrections, PI gains, PWM bounds or the every-loop endpoint check. It
does not claim this endpoint offset works for other distances or surfaces.

The existing speed profile still reports wheel speeds above the nominal low
speed targets near the endpoint. Endpoint fitting does not establish that speed
tracking is resolved. General distance scaling, speed tracking, other lengths
and integrated turns remain separate validation work.

Host sanitizer tests passed with the 3537 straight endpoint and unchanged turn
targets, PI first-update checks, between-tick cutoff, cutoff retention, rollover,
independent wheel completion and all fault paths. The full Pico W build passed.
The versioned UF2 and verification report are under `build/motion-integration/`.

**Pending:** flash v3, repeat one measured `f` run using the same marker and
surface, and preserve the measured distance and STOP/SETTLED lines. No v3
physical accuracy claim is made yet. Flat-floor hump robustness is also open;
no terrain thresholds were changed or false detections suppressed in v3.
