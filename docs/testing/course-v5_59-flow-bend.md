# v5.59: centre/right preview and bounded rolling bend correction

## Five v5.58 physical runs

The user supplied five downloads in chronological order and identified the
route as full A, half A, right bend with half Z. Only run 3 was reported to
traverse the bend and half Z. The logs contain cumulative older events and
repeated recovered snapshots. This table uses each file's first recovered
CURRENT RUN result and the events after its final MOTION GO, not older runs.

| Run | A events, seconds | Final sequence | Rejected captures | Recoveries | Trims | Final stop |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 9.593 / 20.101 | AA | 5 | 8 | 11 | CENTER_LOCAL_RECOVERY_EXHAUSTED |
| 2 | 10.091 / 20.111 | AA | 2 | 15 | 16 | CENTER_LOCAL_RECOVERY_EXHAUSTED |
| 3 | 9.673 / 19.705 | AA | 3 | 23 | 29 | OBSTACLE_STOP |
| 4 | 8.817 / 18.883 | AA | 2 | 7 | 9 | CENTER_LOCAL_RECOVERY_EXHAUSTED |
| 5 | 9.613 / 19.685 | AA | 2 | 10 | 13 | CENTER_LOCAL_RECOVERY_EXHAUSTED |

Given the stated physical ordering, this is full-A 5/5 and half-A 5/5. The
decoder reports A, not printed size. No run records Z; it would be misleading
to call this five failed Z traversals, since only run 3 was reported to cross
it. Run 3's later obstacle stop is consistent with the user's longer traversal,
but the controller has no geometric course-completion sensor.

All five have zero sampling integrity faults, waveform timing faults, FIFO
overflow and waveform recording exhaustion. Maximum ADC gaps are 588..609 us.
All five report **zero rolling-bend attempts**. Thus the driver repeatedly
used stopped/pivot recovery rather than the rolling bend routine. All five
frozen exports are the second accepted A; the v5.58 snapshot preference hid
later failed bend-area captures. Checksums and exact batch replays of those
five A inputs pass, as does the previous successful half-Z input.

Inputs are archived as `logs/20261106-v5_58-AA-bend-run1.txt` through `run5.txt`.
None contained credential fields. The original Downloads files were not edited.

## Decision

Keep GP26 dedicated to barcodes and keep the user's current GP28 centre / GP27
right mounting. These two front sensors are already used, but their current
placement is asymmetric. Two white readings cannot establish left versus
right. A symmetric straddling arrangement could supply bilateral direction
information, but moving sensors would change alignment/calibration and is not
necessary for the next right-bend experiment.

Use the existing centre/right pair with a lower, time-confirmed analogue
right-side hint, confirmed-direction history and bounded forward correction.
Do not replace the whole line controller or change the barcode width rules.
This choice directly addresses two observed/code-proven restrictions:

- v5.58's barcode-near gate prevents rolling-bend arming/continuation and caps
  recovery at 60 counts. Independent waveform capture no longer needs that
  motion restriction, but the restriction remained in the line controller.
- The existing GP27 calibration is white141 / black842. Its old binary
  250/1000 black threshold is approximately raw316. Run-scoped GP27 ranges
  peak at 355/364/339/315/330, with most logged curve-side status values below
  raw316. A useful side hint can be weaker/earlier than a full black decision.
  These status records do not prove exact sensor-footprint geometry.

## Controller change

The new mode is enabled only for the centred **course** run. The obstacle
controller continues to use its existing non-flow behaviour, including its
right-first rejoin policy. Legacy configurations remain available in tests.

- GP27 preview: enter at normalised100 (~raw211), release below60 (~raw183),
  only while GP28 is weaker than85% of its stopped strong reference. Require
  30 ms continuous side evidence. Strong GP28 ignores side-grey background.
  These are provisional engineering thresholds using the existing calibration.
- Confirmed preview can override a guessed left trim. Direction history from
  actual optical improvement with directed encoder yaw, or confirmed recovery,
  remains available. White/white does not manufacture a direction.
- Arm after established strong forward following (existing 300 ms and 200
  counts per wheel). A fresh direction hint can start a forward arc below65%
  centre strength, before the old complete-loss threshold is reached.
- During a committed arc, retain its direction/origins. A contradictory
  confirmed right-side hint during a left arc brakes into recovery instead of
  flipping the arc's sign/budget. Both strong sensors can finish a moving
  reacquisition; they do not force a premature stopped search mid-arc.
- Outer target600 cps; initial differential220 cps, then differential480 cps
  when weak (inner target120 cps). Strong tape tapers steering to100 cps.
  Commands remain forward-only. Actual wheel response/traction needs testing.
- Existing absolute arc limits remain:650 ms,540 counts per wheel,400
  differential counts, bounded reverse/wrong-yaw detection. Flicker cannot
  renew these budgets. Strong centre for40 ms ends the arc while moving.
- Unknown direction after an armed straight segment permits at most80 ms or
  80 forward counts to obtain a side hint, then brakes into the existing
  bounded search. Startup, explicit override and exhausted trim do not obtain
  this grace. No indefinite blind forward drive is introduced.
- Barcode activity no longer disables these course arcs or shrinks their
  fallback to60 counts; fallback uses the normal120-count search. Explicit
  obstacle/rejoin-specific overrides and existing stall/control/sonar stops
  remain. Hardware stop latency/clearance is not established by these limits.
- A decoded mapped Y/Z action during an active arc is deferred until moving
  reacquisition. STOP still acts immediately. An immediate deferred turn
  removes forward output on the same controller step. For initial tests leave
  Y/Z straight.

The GP26 reader remains the v5.58 independent timed waveform reader. Forward
arcs reduce stop/reverse disruption, but do not prove sensor-path-consistent
barcode widths on a physical curve. No expected A or Z is supplied to it.

## Evidence and logging

Retained COURSE RESULT now includes flow mode, arc attempts/rejoins/fallbacks,
right preview count, short forward-window count, direction source and arc exit
reason. Live status adds the GP27 analogue level and active preview. This lets
the next run distinguish no hint, wrong direction, and time/travel/yaw limits.

Frozen waveform selection now keeps any accepted frame or a later substantial
capture of at least1 s; short later noise does not displace an existing useful
snapshot. This allows a failed Z-area scan after successful A to be exported.
The selection is explicit in the snapshot header. It still stores one capture,
not all three waveforms, and a later long noise capture can be selected.

## Verification

- New flow-controller fixtures: timed right preview, pre-loss right arc,
  confirmed left memory, contradictory hint fallback, strong-pair moving
  rejoin, short-spike rejection, unknown-direction forward-window bound,
  time/travel/yaw/reverse/override bounds, deferred action and immediate STOP.
- 52 production centred-main scenarios, including Z decoding during an
  analogue-preview rolling correction. Existing scenarios now explicitly
  require short gaps to pass without stopped recovery, and normal120-count
  fallback instead of barcode-only60. 18 legacy-main scenarios pass.
- Existing centred steering/bend regressions and obstacle planner/alignment/
  state-machine tests pass; no real obstacle was approached by the agent.
- 832 waveform matrix,52 prefix cases,3000 noisy frames and snapshot replacement
  regression. Existing15 barcode/motion suites and packed-storage/ARM checks.
- All five supplied frozen A inputs and the earlier successful half-Z input
  verify their full checksums and decode through current production C code.
  `tools/replay_waveform_log.py` reproduces this batch-pattern check; it does
  not simulate a car's trajectory or restore missing quiet zones.
- Wireless retained results, chunk export/core1 and mocked-browser controls,
  complete/corrupt/changed-capture downloads and mobile checks pass.
- Strict Pico W build and complete UF2/page/version/family/address verification:
  `firmware/releases/build-report-v5_59.json`.

No physical v5.59 run has been performed. The thresholds, arc authority and
right-bend result remain candidates for supervised validation.

## Next run

Flash `robocar_motion_integration_v5_59_wireless.uf2`; reconnect and reload the
page; verify v5.59. Keep the sensor mounting, full A, half A and right-bend
half Z unchanged. Choose Follow track + read barcodes with Y/Z straight.
Run once, stop with GP20 after the final white area, and download before
another run/reset. The targets are AA retained, a moving correction rather
than repeated brake/pivot/relaunch, and ideally AAZ. Check the new retained arc
counters and the frozen waveform even if Z fails. Compare one run before
spending time on another five-run series.
