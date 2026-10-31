# v5.4 third physical barcode pass — 28 October 2026

Source: `serial-output/20261028-v5_4-third-barcode-pass-roll-off.txt`.
User confirms the car traversed the entire barcode and then rolled off the short
track. Do not repeat the 8000-count moving pass on this short platform.
One captured start; recovered/live results duplicate the same run. Boot unchanged
from previous run. Physical sheet identity and placement photograph still absent.

- Stop condition was BARCODE_BOTH_LINE_BLACK, at 7202/7152 normalized counts.
  This is both central channels exceeding black thresholds, NOT cliff detection.
  At stop all three channels read high; reported pitch reached -48.9 degrees.
  This is consistent with the reported roll-off but not an accurate static angle
  measurement during impact/motion. Neither sonar nor pitch stopped the motors.
- No letter: 15 total barcode transitions, no complete 29-width frame, one
  750 ms edge timeout. Final runs=2 is the latest restarted partial capture,
  not the total run's bar count. Its widths are 37000/319000 us.
- GP26 raw 173–2367, 2292 samples, max gap 1012 us, ADC max103 us, queue peak1.
  No overflow or sample integrity fault. Main loop max36408 us does not imply
  the alarm-owned barcode acquisition had that gap.
- Settled counts7388/7335, invalid0/0 relative to start. Cumulative right invalid1
  is inherited from the preceding run; no new encoder error in this pass.
- Sonar range147.4–214.6 cm, one no-echo. No frontal-obstacle stop triggered.
- Earlier transitions versus those produced at roll-off cannot be fully separated
  from retained summary. Do not attribute all 15 transitions to printed bars.

Conclusion: user-reported full traversal still did not produce a full optical
sequence. Do not increase travel limits or disable protections. Request a top-down
photo including printed pattern, line, all three sensors and starting orientation,
plus usable track length. Further moving tests need a continuous level extension
or a floor-level setup. If space cannot be extended, use a future motor-disabled
capture mode; current stopped live logging is not a full barcode-decoding mode.
No firmware edits, flash or motion performed by agent.
