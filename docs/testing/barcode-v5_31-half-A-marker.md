# v5.31: bounded outer marker tolerance

Source: `AAI10-motion-2026-10-30T12-14-13-109Z.txt`, read in this chat. The exact 29 logged widths are frozen in `firmware/tests/barcode_recorded_fixtures.h`. The download contains an earlier aborted run and the later barcode pass; recovered results duplicate the later result. Do not count them as independent captures.

The later v5.30 pass reported reads=0, rejected=1, timeouts=0, saved_runs=29, paused_fragments=0, max_gap_us=1032, integrity_fault=0. The first marker and payload classify as `*A`; only the final marker fails width coherence/separation. Its terminal narrow black interval is125000 us versus nearby black narrow intervals40999/42001 us. All its other elements satisfy existing checks. This identifies the rejection, not the physical cause of the expanded interval. The min/max optical windows cannot establish an exact raw-sample history; speed variation, threshold crossing and geometry remain possible causes. No missing bar is inferred or inserted.

## Change

Strict decoding is attempted first. A fallback permits one expanded physical first/last narrow BLACK guard bar only. It must still rank as narrow, be longer than the other narrow runs, remain at most three times their corrected median and at most80% of the shortest corrected wide interval. The remaining guard elements, payload, separators, positivity/range and1.8 wide:narrow separation checks remain strict. Both guard patterns must spell `*`, and payload must be A-Z. Conflicting candidates are rejected. Two distorted outer bars, interior marker distortion or payload distortion are not repaired. The bounds are provisional engineering tolerances supported by this replay, not evidence of broad physical robustness. A malformed waveform that satisfies these bounds can still be accepted; this is a deliberate, narrowly scoped relaxation, not recovery of information that was never sensed.

No expected-letter input, A-specific branch, change to sampling thresholds226/220, mean10 filter, motor targets, steering, recovery, sonar or course actions. Stream still requires250ms white before another capture. It emits after29 runs as before; it does not newly validate a trailing quiet zone. Extra activity after a decoded frame is ignored until rearm, not used as evidence to fix the frame.

## Validation

Exact logged frame now decodes A in both directions, through packed and unpacked APIs and the actual continuous acquisition pipeline. One event, no early event and no duplicate from trailing short activity. Prior recorded A/Z passes retained. Synthetic A-Z matrix across both directions, scales and colour biases; single-boundary expansion; inclusive80% and3x bounds; over-limit, double-boundary, interior distortion, exhaustive guard masks, malformed/weak frames and sampling failures pass under sanitizers.32 centre-main and18 legacy-main scenarios pass; strict Pico W build and UF2 headers/family/address/version checks pass. No robot was operated or flashed.

Physical next step: one half-A drive with v5.31 at the same mounting. Look for `[BARCODE EVENT] ... letter=A` and final `reads=1 sequence=A`. Then validate full/half Z and full A before claiming size-independent barcode support. Preserve steering v5.30 so this test isolates decoder acceptance.
