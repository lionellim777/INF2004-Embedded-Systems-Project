# v5.11 Z/A follow-up —28 October2026

Sources: `serial-output/20261028-v5_11-Z3.txt` and `serial-output/20261028-v5_11-A4.txt`. Retained-result block duplicates the last live result; count only live run boundaries. Both exports share boot5303e3f9. User describes first file as3 successful Z runs and second as A pass/pass/fail/pass with third intentionally angled out of barcode. Actual logged outcomes below distinguish stopping from successful decoding.

|Run|Decoded|Stop|Edges|
|---|---|---|---|
|Z1|None|BARCODE_REJECTED|30|
|Z2|Z|BARCODE_READ|30|
|Z3|Z|BARCODE_READ|30|
|A1|A|BARCODE_READ|30|
|A2|None|BARCODE_REJECTED|30|
|A3|None|BARCODE_CAPTURE_TIMEOUT|2|
|A4|A|BARCODE_READ|30|

Four accepted decodes across seven recorded attempts. A3 is user-reported deliberate misalignment and should be separated from normal-alignment repeatability, not relabelled as a barcode success. It captured one62ms black element then no further transition before timeout, consistent with exiting the barcode area. The report cannot certify the physical cause solely from logs.

Failure stopping is now physically demonstrated: Z1/A2 stop on rejection at about3.36/3.38s capture, A3 on timeout at1.045s, without continuing to the old BOTH_LINE_BLACK stop. All runs have zero encoder-invalid counts and zero sampling gap/overflow/integrity faults. Maximum sampling gaps1015–1032us. Successful scans coexist with recorded line-steering intervals, including right-side correction in A4; no whole-course navigation validation implied.

Remaining rejection cause can be reproduced from saved29 widths. Decoder requires smallest wide/largest narrow>=1.8 independently per9-element symbol. Z1 ratios1.745/1.643/1.880; A2 ratios1.673/2.000/2.243. All30 transitions were captured, so these failures are classification rejection, not lost sampler updates. Widths in these scans show black/white asymmetry; possible optical threshold bias and speed changes need distinguishing. Do not blindly relax acceptance just to pass known A/Z labels. Investigate per-colour or pattern-aware width validation with negative/ambiguous-pattern regressions using recorded traces before requesting more physical runs.

No firmware changes in this analysis turn; current delivered build staysv5.11. Physical setup unchanged. Next work: offline decoder investigation using these complete traces, retaining stop-on-failure behavior and standards-valid guard/data checks.
