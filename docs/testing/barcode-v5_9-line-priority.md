# Two successful A runs and line correction regression — 28 October 2026

Physical evidence: `serial-output/20261028-v5_8-A-two-successful-runs.txt`,
boot488668b1. Recovered latest result repeats run2; there are two actual runs.

| v5.8 run | Decode | Edges/frames/rejections | Settled L/R | Invalid L/R | Max sample gap |
|---|---|---|---|---|---|
|1|A valid1|30/1/0|3726/3671|0/0|1027us|
|2|A valid1|30/1/0|3728/3649|0/0|1016us|

Both automatically stopped BARCODE_READ. No capture overflow, gap, integrity,
trace or stale fault. Recorded capture durations1622/1624ms. This establishes
two physical A successes at the current print/setup; Z and course readiness
remain unverified. Generic CHECK IR_L edges12 uses old normalised diagnostics,
not the dedicated GP26 barcode sampler: the authoritative barcode results have30.

User reports the car no longer follows the strip. The log confirms central-left
GP28 detects black (up to2908/2927); central-right GP27 remains149–173 and white.
BARCODE_CORRECT_LEFT is requested repeatedly but v5.8 outputs around0.29/0.31,
much weaker than the earlier line-check0.25/0.55. v5.8 softened the correction
to a+/-200cps target bias, updated only each100ms. Actual running speeds are
roughly2200–2500cps despite base target1200; do not claim verified9cm/s control.

v5.9 restores the established direct line-check response in barcode mode:
left edge -> left0.25/right0.55, right edge -> left0.55/right0.25. Applied on each
foreground step, before waiting for the next100ms velocity update. Safety checks
still precede output, including both-black stop. Velocity integral is frozen
for intervals containing steering so it cannot counteract the override; straight
position balance uses a new anchor after intentional steering. Measured cps is
still updated. Clear/clear retains the existing velocity loop, whose actual-speed
tracking remains pending; the page explicitly states that limitation.

Barcode threshold251/220, sampler/decoder, calibrated manoeuvre controller,
8s/8000-count bounds, GP20/browser stop, sonar and fault guards preserved.
v5.8 successful artifact retained. New artifact:
`build/motion-wireless/robocar_motion_integration_v5_9_wireless.uf2`.

Validation: strict Pico W build; new immediate left/right switching, return to
base drive, frozen integral under measured v5.8 speed conditions, and stop override
tests; existing barcode/motion/line/guard regressions; wireless core and both
transport variants. UF2 format/version/family checked; v5.8 artifact and decoder,
acquisition and calibrated motion source hashes verified unchanged. Details in
build-report-v5_9.json. No robot operations or flashing performed by the agent.

Next user run: flashv5.9, refresh page, select Barcode pass, test Z on the same
flat continuous setup and observe line correction. This combines next-symbol
checking with correction validation. Neither successful Z nor physical steering
restoration is claimed before that evidence. A should also be repeated on v5.9
before declaring repeatability complete.
