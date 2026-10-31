# v5.4 second physical barcode pass — 28 October 2026

Source: `serial-output/20261028-v5_4-second-barcode-pass.txt`, supplied by user.
Boot 90b321f6; one captured motion start. Recovered/live results are duplicates.
The sheet identity, positioning and whether its full length passed GP26 were
not supplied. This is a moving pass, not the requested stationary contrast hold.

- GP26 capture range 176–2396; 12 transitions and 11 completed run durations.
  Optical black/white contrast now crosses the 935/800 thresholds. No need to
  lower thresholds based on this evidence. No full 29-run frame or decoded letter.
- Capture samples 1272, max gap 1007 us, max ADC callback 99 us, queue peak 1;
  overflow/gap/integrity flags all zero. The 250 ms text snapshots show only low
  GP26 readings but the 1 ms acquisition and foreground summary captured highs;
  sparse text values must not override the dedicated acquisition result.
- Stop: BARCODE_ENCODER_FAULT at forward-normalized counts 3818/3817.
  Settled 3969/3964, invalid=0/1: one right encoder illegal A/B transition.
  Current barcode_drive_step stops at the first invalid transition. Cause is
  unknown: electrical noise/contact, missed edge/IRQ latency or sampling of the
  two encoder GPIOs at different instants remain hypotheses, not diagnoses.
- GP28 summary 166–2649, 13 transitions; GP27 144–163, zero transitions.
  Could be normal route corrections or unintended barcode overlap. Need physical
  placement evidence before changing the established channel mapping.
- Sonar near end: 28.3 cm immediately after stop, 26.4 cm later. Encoder fault
  caused this stop; a clearer forward route is needed for the next complete pass.
- No firmware changes or relaxed fault checks. Next: one large-A retry with
  clear continuous route and GP26 crossing every bar. Ask which sheet was used
  and whether the car stopped before leaving it. Repeated encoder faults require
  targeted encoder/interrupt investigation, not repeated barcode-size trials.
