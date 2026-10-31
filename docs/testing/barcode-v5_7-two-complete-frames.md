# v5.7 two complete hand-scan frames — 28 October 2026

Source: `serial-output/20261028-v5_7-two-complete-rejected-frames.txt`.
Current boot5e04775e is v5.7. The browser download heading still saysv5.6 and
live text includes the old boot's failed capture; use the recovered current-boot
result and final v5.7 capture, not the stale history.

Current capture:45.001s,45001 samples,60edges,2frames,2rejected,0timeouts,
raw177–2727,max gap1013us,ADC maximum37us. No gap, stale, overflow, integrity,
or trace fault. Each swipe contains30 edges/29 run widths, matching a complete
15-bar symbol. This is progress from six detected black regions per swipe.
Neither frame was accepted as a letter by the running firmware.

Replay through the actual C decoder also rejects both captures. Ranking the
three widest elements per symbol gives reversed *A* in both frames, but ranking
alone omits the decoder's required wide/narrow separation. At least1.8 separation
is required for each symbol. In reverse reading order, measured separations are:
- Frame1:1.6275,1.8773,1.8125; first marker fails (498ms/306ms).
- Frame2:1.8485,1.2281,1.3248; middle and last symbols fail.

First-to-last-edge durations:11.122s and17.638s. These are capture measurements,
not a correction of the user's description of intended swipe speeds.
Replay details: `build/stationary-host-tests/v5_7-physical-replay.json`.

Variable hand speed and threshold-dependent optical edge positions can distort
duration ratios; this log cannot separate their contributions. Do not blindly
loosen the ratio to force these two known-A samples through. Sampling/fault fixes
worked for this run; reliable barcode recognition is still unproven.

No more identical hand sweeps needed immediately. Recommended next stage is a
bounded controlled-drive A pass with fixed sensor mounting and sufficient flat
run-out, followed by Z, then repeatability checks. A proposed initial acceptance
screen is three accepted passes each of A and Z at the selected size/speed, six
successful runs total; failures require diagnosis, not a guaranteed test count.
This is a local calibration milestone, not full-course acceptance. No firmware
change, flashing, or robot operation was performed for this analysis.
