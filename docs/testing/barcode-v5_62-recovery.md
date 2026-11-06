# v5.62: retry lost confirmation; bounded waveform-width consensus

## Two supplied v5.61 runs

Use each file's current-run result: the second export also contains the first
run's timeout and old retained results. Do not count those as a second timeout.
Archived originals: `logs/20261106-v5_61-evening-run{1,2}.txt`.

| Run | Accepted | End reason | Recoveries | Hard arc attempts |
| --- | --- | --- | --- | --- |
| 15:26Z | A | CENTER_CONFIRMATION_TIMEOUT |16 |0 |
| 15:28Z | AA | OBSTACLE_STOP |19 |0 |

Run1's A capture spans7.096s and13mm lateral excursion/10deg yaw. The user
observed line recovery while scanning it. The half-A attempt has31 extrema,
framed094/109/094 ranks and width failures4/4/0 (separation). No Z was reached,
according to the user. A later1.156s paper-noise capture replaced the useful
half-A full waveform, although its diagnostic widths remain in the ledger.

Run1 did seek the line. At the final search it briefly found strong tape and
entered zero-power CENTER_STOP_CONFIRM. Settling left GP28 around559raw,
normalised139. The held-black requirement was70% of reference752, about526
normalised (roughly1614raw). It therefore could not confirm. After300ms the
old routine terminated instead of resuming the remaining bounded search.
This was an optical confirmation failure, not encoder/ADC failure or a motor
service timeout. Stopped telemetry after the fault is not evidence of a new run.

Run2 accepted full A and half-A. Its later half-Z waveform has2462 readings,
FNV9a48ce7e,31 extrema at both48 and96ADC prominence, and reversed patterns
094/0d0/094. The payload Z passes; both star guards fail worst-case separation.
There is no timing/queue/motion-path fault. The recording spans65mm lateral
excursion and20deg yaw, so the fixed-axis geometry remains approximate.

The old check demands smallest corrected wide >=1.8 * largest corrected
narrow. Around a2:1 print, ordinary within-class variation can violate this
even if the class means and all element ranks remain coherent. Here the
three mean ratios are2.25/2.48/2.48; corrected narrow residuals are about
0.67..1.31 of their means and wide residuals0.80..1.14. Changing contrast alone
(midpoint contour sweep10..90%) did not recover the old strict decode.

## Implementation

1. Flow-follow recovery only: a black candidate that disappears during stopped
   confirmation can return to search after80ms, provided the wheels have been
   still50ms and the candidate has stayed absent30ms. This returns zero power for the transition tick. Original sweep
   origins, stage deadline, total3s deadline and travel caps remain. If coasting
   already consumed the sweep, the next tick reverses or exhausts normally.
   Legacy/obstacle and explicit commanded-turn searches keep their old policy.
   Excess coast, stall, late control and actual budget exhaustion can still stop.
2. Live, timed right preview can authorize the existing bounded arc after a
   successful recovery and established run travel, even if repeated short
   segments starved the300ms/200-count arming period. Launch must be complete;
   the centre must be weak/off, and CURRENT GP27 evidence must still exist.
   History alone cannot start an arc. Existing650ms/540-count/400-yaw budgets
   and STOP/motor guards remain.
3. Keep the original Code39 API and legacy acceptance unchanged. Waveform-only
   fallback checks corrected mean wide/narrow ratio2..3.5, every narrow within
   +/-35%, every wide within +/-25%, smallest wide >=1.3 * largest narrow, plus the
   existing2x class-spread checks, correct guards and narrow separators. It
   allows no missing/inserted bars or outer-bar exception. Only exact31-extrema
   frames with at least TWO agreeing prominence profiles can use it. Conflicts
   still reject. This is a deliberate, bounded relaxation of the old worst-case
   ratio, and needs false-positive testing on the physical track. The profiles
   are processing alternatives from the same measurement, not independent scans.
4. Log confirmation_retries and coherent_mask. Strict width diagnostics remain
   visible even when the waveform consensus model accepts a frame. Retain a
   high-contrast saved waveform over later low-contrast paper noise. The per-
   attempt ledger remains available; only one full waveform is stored.

## Evidence and next test

The checksum-verified run2 frozen waveform now batch-decodes Z with the48/96
profiles agreeing. Its new binary fixture is tracked. Run1's failed half-A
widths pass the model as A, but its raw waveform was overwritten: two-profile
acquisition acceptance for that attempt cannot be proven retrospectively.
A batch decode does not replay steering, timing or route actions, and is not
physical v5.62 success.

Added tests reproduce candidate-loss retry, exhausted-budget reversal, eventual
reacquisition, total deadline preservation and live-preview arming starvation.
The optical regression also rejects510 invalid guard pairs and a1.4 ratio,
while retaining the existing waveform/negative/noise and actual-main suites.
See the build report for checks performed and the exact UF2 hash.

Flash v5.62, keep Y/Z Straight, and run the unchanged full-A / half-A / right-
bend half-Z course once. Download before starting the next run. Target AAZ;
inspect confirmation_retries, arc_attempts and coherent_mask. Then repeat the
same setup several times and include blank/nonbarcode passages to check false
acceptance. Physical reliability, wheel slip, printed skew and unobservable
all-white direction remain unresolved. No100% bend/barcode guarantee is made.
