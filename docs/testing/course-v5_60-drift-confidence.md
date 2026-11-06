# v5.60: drift correction is not a confirmed bend

## Evidence from the two v5.59 straight-section runs

Source: `logs/20261106-v5_59-straight-drift-two-runs.txt`. The user reports two
tries on a straight section before the bend. This download contains two actual
MOTION GO sections and repeated retained results; do not count the repeated
snapshots as extra runs.

| Run | Stop counts | Reads | Recoveries | Hard arcs | Rejoins | Preview confirmations | Arc source | Arc exit |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 4965 / 4814 | 0 | 1 | 1 | 0 | 0 | CONFIRMED_RECOVERY | TIME_LIMIT |
| 2 | 4230 / 4022 | 0 | 1 | 1 | 0 | 0 | CONFIRMED_RECOVERY | TIME_LIMIT |

Both stop at CENTER_LINE_NOT_REACQUIRED. Neither reports a sampling integrity
fault. GP27 never reaches the old full-black threshold in the retained CHECK
ranges:91..299 and102..280, respectively. No 30 ms analogue preview was confirmed.
During the hard right arcs, sampled GP27 values remain around white (roughly
119..153). These are not measured right-edge entries.

The code explains this result: v5.59 allowed a recent search/recovery direction
to satisfy the arc-entry evidence test. After finding the line with a right
search, the next centre drop could therefore trigger the 480-cps differential
arc without current GP27 evidence. It then stayed on that arc until the650ms
limit. This is a demonstrated software confidence error, not evidence that
the physical straight track actually bent.

The initial lateral drift is a separate unresolved physical/control question.
Pre-loss wheel counts in the log are often nearly balanced. Equal encoder
rotation does not establish heading/position relative to the tape, or identical
effective wheel circumference. The existing centre/right sensor pair cannot
measure where the line is on the uninstrumented left when both see white.

## Change

Only the centred course flow mode changes; legacy and obstacle flow-disabled
behaviour retain their existing parameters and right-first rejoin handling.

- A hard right tracking arc now requires the CURRENT 30 ms GP27 preview plus
  right direction. Even a single full-black GP27 sample cannot bypass that
  confirmation. Recovery direction, old successful trim direction and loss of
  GP28 alone cannot start a hard arc in this mode.
- A recovery direction is labelled RECOVERY_PRIOR. It may seed a small
  correction/search trial, but it is not a geometric bend classification.
- Gentle centre correction begins below90% of the stopped reference (was85%)
  and releases at95% (was92%). Its differential target is40..150 cps (was60..220).
  The outside target remains600 cps. This means earlier, smaller optical
  correction rather than a480-cps escalation from memory.
- The existing gradient check remains: evaluate optical improvement only after
  actual directed encoder yaw. Reverse once on deterioration or stalled
  improvement; taper out on strong tape. Existing overall time/travel, control
  lateness and encoder protection remain. The controller does not learn a
  direction merely because it requested it.
- The v5.59 brief unknown-direction window, bounded search, valid measured-right
  arc budgets, continuous barcode acquisition and immediate STOP/deferred-action
  rules remain. Physical stop distance and optical curve resolution are unverified.
- Retained COURSE RESULT includes `arc_gate=TIMED_RIGHT` and `drift_max_cps=150`
  in flow mode, so a future log can distinguish the new policy from v5.59.

This does not claim to identify the geometric difference between every drift
and bend. Its purpose is to steer toward measured tape with proportionate
authority, and avoid treating missing observations as evidence for a strong turn.
A symmetric pair straddling the tape would provide better bilateral position
information, but would require physical mounting and calibration work. No sensor
was moved or assumed to have become a left sensor in this change.

## Validation

- New negative regression: fresh left/right recovery prior, centre fades and
  GP27 stays white; no hard arc. A full-black short spike also cannot start one.
- New gentle-feedback regression: a small initial right trial produces measured
  yaw but worsening reflectance, reverses to the left, then releases when the
  centre reading improves. The probe stays within150 cps differential.
- Existing measured-right preview still enters a bounded forward arc, including
  a valid Z read during correction. Existing bounds, deferral and STOP checks pass.
- 53 production centred-main cases and18 legacy-main cases. The new full-main
  regression injects a recent recovery prior before a weak centre/white-right
  episode; it keeps zero arc attempts and still records its synthetic Z.
- Existing centred bend/steering, barcode/motion and obstacle regressions pass.
- Strict Pico W build, retained results, checked waveform export and mocked
  browser controls/download/mobile checks pass. See release report for hash,
  sizes and full UF2/page/version verification.

These are software checks and synthetic optical/controller scenarios. No
physical v5.60 straight run or bend pass has been performed by the agent.

## Next physical check

Flash `firmware/releases/robocar_motion_integration_v5_60_wireless.uf2`, reconnect
and reload the page, and verify v5.60. Keep the current mounting unchanged and
Y/Z straight. First repeat only the same straight section, then download before
another start/reset. Check whether the small corrections preserve the line and
whether `arc_attempts` stays zero when `previews=0`. Compare one straight run
before extending the test to the bend.

The earlier v5.58 full-A/half-A successes and single straight half-Z success
remain historical evidence, not proof that these modified driving parameters
preserve physical decoding. The unchanged waveform reader still needs full
framing and trailing white.
