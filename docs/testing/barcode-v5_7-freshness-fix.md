# v5.7 stationary freshness check — 28 October 2026

User reports two physical swipes, first slower. Source log:
`serial-output/20261028-v5_6-premature-data-fault.txt`, boot48202358.
Only one capture starts. It stops with SCAN_DATA_FAULT at6492ms,6493samples,
zero edges, raw181–210, max gap1012us, ADC maximum37us, no buffer/trace overflow.
After the stop, idle readings show the black/white sweeps. Neither swipe was
captured for decoding; do not assess the new251/220 thresholds from this run.

Code inspection found a race in sample_fresh: time_us_32()-last_sample_us can
subtract a timestamp updated by an ADC interrupt after the clock read. Unsigned
age then wraps and triggers a false stale-data fault. Scenario16 reproduces this
on v5.6 (saved failure in build/stationary-host-tests/v5_6-race-reproduction.log).
This is consistent with the physical log; the old log did not record the failed
age, so the exact physical interleaving cannot be proven retrospectively.

Fix: save/disable interrupts briefly, read last_sample_us then the current clock,
restore the prior mask, compute age. Nested use preserves the caller's mask.
No timeout threshold is relaxed. Result now includes gap_fault and stale_age_us
to distinguish measured sampling gaps from a stopped sampler and other faults.
The GP26 >250 black / <=220 white setting, cadence, and decoder stay unchanged.

Artifact: build/barcode-stationary/robocar_barcode_stationary_v5_7.uf2.
18 actual-main scenarios pass with all motor writes LOW, including the failing
interleaving regression, nested interrupt masks, timer wrap, genuinely stale
samples and a halted sampler. Existing measured-level A/Z scenarios also pass.
Pico W strict build and wireless core/ordinary/stationary transport checks pass.
UF2 format/version/family checked; motion/encoder/terrain symbols absent.
v5.5 and v5.6 artifacts preserved. Hashes in build-report-v5_7.json.

Physical validation remains pending. User flashes v5.7 and performs one full
large-A swipe after SCAN READY, finishes if still active, then downloads. No
new mounting adjustment, threshold change, automatic flashing, or motor movement
was performed by the agent.
