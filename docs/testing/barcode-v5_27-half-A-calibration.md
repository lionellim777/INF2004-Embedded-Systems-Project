# v5.27 half-size barcode calibration trial — 30 October 2026

## Evidence

Latest capture only in each supplied download was extracted. Downloads contain previous captures and a duplicate retained summary; these were not counted as extra independent trials.

| File suffix (AAI10-motion-2026-10-30T) | User-reported sequence | Raw range | Max sample gap |
|---|---|---:|---:|
|11-08-57-188Z.txt|white / narrow black / narrow white / wide black / narrow white / narrow black / white|203–3358|1016 us|
|11-09-15-408Z.txt|same three-black sequence|207–3372|1010 us|
|11-10-16-427Z.txt|white / narrow black / narrow white / wide black / narrow white / wide black / narrow white / narrow black / white|194–3588|1011 us|
|11-10-36-217Z.txt|same four-black sequence|190–3043|1009 us|
|11-10-59-599Z.txt|same four-black sequence|172–2301|1011 us|
|11-12-01-815Z.txt|entire half-size A|167–2448|1009 us|

All six latest captures have5000 samples,500 populated10 ms windows and no abort. User is unsure whether paper height/angle remained constant. The early captures show some white valleys still hundreds or even over1000 ADC units after large peaks; the full half-A capture has white and narrow peaks near200–240. This is not a consistent calibration across all six runs.

## Analysis and limits

Using each window midpoint as a reconstruction, the old251/220 hysteresis finds6 black segments in the full scan, consistent with the six wide bars of a framed Code39 letter, versus the15 black bars needed. Lower thresholds can recover30 transitions and a strict A decode. A sweep of absolute thresholds produced no pair matching the reported stripe counts across all six midpoint reconstructions. Matching a stripe count alone is not proof of correct boundaries.

There are62 midpoint threshold pairs that yield A through the unchanged production decoder. This is exploratory calibration, not62 independent successful tests. Low-only and high-only window reconstructions have different suitable thresholds: no common absolute pair passes all three. Raw1 ms ordering/mean was not retained, so exact physical replay is impossible.

## Implemented trial

The moving scanner uses a trailing10-sample integer mean at the existing1 ms acquisition rate, black entry226 and white release220, followed by the existing two-sample edge confirmation. Filter storage is bounded; no new allocation, scan speed or ADC timer. Timer faults and strict29-run Code39 guards/width validation remain. No expected letter is supplied to the decoder. GP28/27, line controller parameters, speed, motion limits and mission actions unchanged. More sensitive GP26 activity can affect when the existing barcode-nearby recovery bound applies; its policy is unchanged.

Raw optical diagnostics remain unfiltered. The archived stationary scanner keeps its original251/220 defaults via barcode_sampler_begin; moving main explicitly selects226/220. The new profile is calibrated for the latest full-half-A trace, NOT a universal solution for every earlier optical condition.

## Validation

A fixture preserves all six500-window extrema traces. Tests construct midpoint, alternating minimum/maximum, rising and falling intra-window sequences. The actual new filter, sampler and stream decoder recover A once for each full-half-A reconstruction. Partial sequences produce no accepted letter. These assumptions about samples are explicitly synthetic, not measured1 ms replay. Synthetic A–Z strong-contrast frames in both directions at30/60 ms narrow-element durations pass, as do white-noise rejection, filter bounds, malformed calibration and prior barcode/motion suites.32 centred production-main scenarios pass. Existing acquisition unit tests retain the legacy default profile; the new filtered tests explicitly exercise the moving profile.

The10 ms mean can blur very fast narrow features;30 ms is the shortest narrow-element duration covered by the new filtered A–Z matrix. Do not infer validity at arbitrary speed/scale. Physical moving half-A acceptance remains pending.

## Next physical test

Use v5.27 with the current sensor mounting fixed, half-A flat beside the tape, clear white before and after it. Choose Follow track + read barcodes, leave Y/Z straight, prepare then start with GP20. Make one controlled drive, stop before lifting, download log. Require BARCODE EVENT letter=A and final sequence=A (or AA if deliberately crossing two A sheets). If it fails, use the retained stop/partial/filtered signal evidence; do not infer success from visual stripes or the earlier optical sampling_ok flag.
