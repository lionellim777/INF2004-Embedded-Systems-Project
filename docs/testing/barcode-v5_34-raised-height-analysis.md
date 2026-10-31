# Raised-height optical investigation, 30 October 2026

## Inputs and observed outcome

Archived `logs/20261030-v5_34-original-height.txt` and `logs/20261030-v5_34-raised-height.txt`. Both are v5.34 physical runs; the user reports route completion, and reports raising GP26 before the second run. Neither contains an accepted barcode. Raised run:18 confirmed transitions,5 timeouts, zero full frames, no sampling faults, no motion or phase capture aborts. Maximum sample gap541 us. Early captures end after4 then1 completed intervals. Final stop is line not reacquired; this is not evidence of a barcode-triggered stop.

## Offline threshold investigation

The raised run retains500 bins with raw minima/maxima per10 ms, beginning after first detected activity. It does not retain the exact2 kHz sample chronology or complete pre-trigger optical history. Exported bins: `logs/20261030-v5_34-raised-height-bins.txt`.

`tools/replay_optical_thresholds.c` tries all black-entry values185..3500 inclusive, hysteresis differences2/7/12, and four within-bin reconstructions (midpoint, alternating extrema, ascending ramp, descending ramp). It uses the production12-sample filter, sampler, independent framed stream and decoder, with a synthetic180-level lead/tail. Result:39792 configurations, zero accepted letters. Some noise profiles create full-length candidates but none validates. This is a sensitivity experiment, NOT a raw physical replay, an exhaustive test of all algorithms, or proof that every true within-bin signal is undecodable. Thresholds above the original trigger may also lose an initial weak bar, so these results cannot establish a new optimal threshold.

The reconstructed midpoint trace has six large pulses, approximately1800..1960. Small intervening features include values around250..260, while some valleys between large pulses remain around275..320. This supports an optical separation/mounting or signal-classification problem; it does not identify a defective sensor or prove the precise cause. Height, footprint, angle and travel path have not been separately controlled. No A-specific template inference or missing-edge invention is acceptable.

Control: production decoder regression still accepts the archived full29-interval half-A capture and nine archived A/Z captures. That demonstrates the existing decoder can decode those captured sequences, not that the current mount produces them.

## Next required information

Asked the user for current paper clearance measured from the downward-facing sensor surface (not PCB) and minimum printed bar width in mm. Exact sensor part/model is not yet verified. Do not invent an optimal height from a different module's datasheet. No new firmware was built: no evidenced software-only correction emerged from this investigation. Keep v5.34 until optical resolution can be established at the actual mounting, or a demonstrably useful acquisition change is available.

Reproduction (from repository root, macOS with command-line tools):

```sh
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk clang -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/app/robot_firmware tools/replay_optical_thresholds.c firmware/app/robot_firmware/barcode_acquisition.c firmware/app/robot_firmware/barcode_reader.c firmware/app/robot_firmware/barcode_decoder.c -o /tmp/barcode-threshold-sweep
/tmp/barcode-threshold-sweep docs/testing/logs/20261030-v5_34-raised-height-bins.txt
```
