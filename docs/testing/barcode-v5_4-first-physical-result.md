# v5.4 first physical barcode pass — 28 October 2026

Source: `serial-output/20261028-v5_4-first-barcode-pass.txt`, supplied by user.
One motion start/go/stop is present. The recovered result duplicates the live
result; it is not a second run. The opening log gap precedes the captured start.
Which sheet was used and whether it passed under GP26 were not stated.

- Stop: OBSTACLE_STOP; sonar readings 28.0 then 26.1 cm, below 30 cm threshold.
- No decode: decoded=-, valid=0; zero barcode edges, frames or captured runs.
- GP26 raw during acquisition: 172–216, below black-ON threshold 935.
- 1311 samples; maximum gap 1039 us; maximum ADC callback 102 us.
  No overflow, sample-gap or integrity fault. Sampling worked in this run;
  this does not establish timing under all loads or barcode optical performance.
- Settled forward-normalized counts 4073 / 4151; invalid transitions 0 / 0.
  No measured physical distance or angle was supplied.
- Foreground summary: GP28 175–2381 with 8 transitions; GP27 145–165 with none.
  GP28 may have crossed the route line or another marking; its transitions do
  not prove that the barcode was placed under the wrong sensor.
- Pitch -8.2 to +10.6 degrees, no new humps/read errors. No causal conclusion
  about acceleration, lifting or surface geometry without physical observations.

Next: retain firmware. With car stopped at normal sensor height, hold printed
white then a wide black bar under the leftmost sensor for two seconds each.
Read the FIRST IR value; establish contrast and channel identity before another
moving pass. If it stays near 200, inspect height/alignment/print response/wiring;
do not lower the threshold to fit noise. Clear the sonar's forward route and
retry large A only after contrast is demonstrated. Other printed sizes pending.
