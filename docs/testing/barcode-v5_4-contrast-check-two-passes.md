# Contrast confirmation and two moving passes — 28 October 2026

Source: serial-output/20261028-v5_4-contrast-check-two-moving-passes.txt.
User reports narrow white gaps100–200 and black above2100 during stationary
checking, then full physical traversal. This supports existing935/800 thresholds
for the checked positions; it does not establish contrast throughout motion.

Two countdown starts were rejected (white/fresh-sample start check failed; log
cannot distinguish the two conditions). Two actual motion starts followed:

1. Right encoder invalid transition: stop1805/1804, settled1950/1956, new invalid0/1.
   Barcode6edges/5widths, no frame/decode/timeout. Max samplegap1038us.
2. Both central line channels black: stop7538/7664, settled7554/7712, new invalid0/0.
   Barcode17edges, one timeout, no frame/decode; max samplegap1062us.
   Last partial capture4widths68000,20000,281000,131000us. Raw170–2742.
   No overflow, gap or integrity flags. Pitch-16..7.9; user has not confirmed
   whether this pass also left the platform, so do not assert another roll-off.

Recovered result duplicates second pass. Neither completed29run widths, so
neither reached full-frame decoding (frames0/rejected0); no wrong letter emitted.
The first-pass encoder error repeats an earlier observed issue and warrants
GPIO/IRQ/contact investigation before further powered barcode passes.

Current retained widths show only latest partial/completed capture; timeout then
restart loses the earlier partial width list. Idle live logging is only ~4Hz,
not a full optical waveform. Next useful diagnostic: motor-disabled capture with
full-run edge/raw evidence and a longer hand-scan timeout, using steady sensor
height. This is not implemented by this analysis. No firmware or thresholds changed.
