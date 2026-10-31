# v5.5 completed stationary captures — 28 October 2026

Source: `serial-output/20261028-v5_5-completed-stationary-scans.txt`, user download 05:44:02Z, boot6389f061.

Two completed captures are present (the recovered latest result also repeats in the live log):
- First capture: SCAN_TIME_LIMIT at45000ms,45000samples,24edges,0frames,0rejections,2timeouts. Edge trace contains two separate12-edge slides, spanning12613ms and8007ms from first to last edge. Raw175–3729; max sampling gap1020us, ADC maximum38us. No overflow/integrity/trace faults.
- Second capture: USER_FINISHED at19542ms,19543samples,12edges,0frames,0rejections,1timeout. Slide spans10145ms. Raw170–2562; max gap1015us, ADC maximum36us. No overflow/integrity/trace faults.

No letter decoded. Each slide registers six black pulses, rather than the15 black bars of a complete supplied *A* or *Z* pattern. The first capture is the completed version of the previously downloaded unfinished two-slide run; the latest capture is an additional slide, not a contradictory12-vs24 counter.

For the final slide the six black pulse durations are495,452,771,633,593,604ms. Internal white durations are299,1994,2933,892,479ms. For reversed *A*, retaining only wide black bars gives equal3-unit black widths and intervening1,7,9,3,1-unit gaps. This resembles the observed short/long/long/medium/short gaps across all three slides. It suggests narrow black bars failing to cross the935 black-on threshold, but does not establish identity, optical cause, or a new threshold. Sparse4Hz raw logs cannot reconstruct the missing narrow-bar peaks.

The recorded sampling cadence does not support increasing polling as the next fix. No frame was attempted, so this is acquisition incompleteness rather than a decoder rejection. Timeouts occur after an incomplete pattern remains on white; extending the timeout does not recover missing edges.

Next targeted measurement: with motors off and paper at its normal mounted clearance, compare idle GP26 readings centred on a wide black bar, a narrow black bar, and a narrow white gap. No new scan or firmware required. Determine whether narrow black clears935 and narrow white falls below800 before changing thresholds or sensor height. Physical driving and barcode acceptance remain unverified.
