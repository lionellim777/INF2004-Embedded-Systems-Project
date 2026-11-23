# v0.4.2 barcode recovery and first-failure capture

## Diagnosis from the v0.4.1 track run

Source session: `serial-output/rtos-network-20261108-234218`. The verified
download contains 19720 retained samples, 19 frame checks, 7 abandoned captures,
35 recoveries and one accepted A at 57776ms, consistent with the user's half-A
observation. Capture fault bits and attempt-overflow count are zero. There is
no evidence that line recovery disabled sampling or exhausted the queue.

The substantial full-A candidate started at 40854473us and lasted 7666000us.
Its best profile has 33 extrema rather than the normal complete frame's 31:
an extra black/white transition pair prevents ordinary complete-pattern
decoding. It spans approximately 9 degrees of estimated yaw and 12mm lateral
travel. The user observed a recovery during full A. The exact source of the
extra transition is not established because its raw waveform was replaced by
the later substantial bend capture. No edge deletion or expected-A guessing
has been introduced to manufacture a result.

The bend capture starts at 63378473us, contains 4491 readings, and spans about
40 degrees/173mm in the decoder's odometry. Its higher-prominence profiles
contain all 31 extrema. The measured seventh interval is 2757 units, below the
old floor of 4000 units (1000 units per mean encoder count). This is the
sensor's *apparent* black width, not a measurement of the printed 3mm bar.
The old floor prevents the existing black/white crossing-bias compensation
and complete Code39 validation from examining that otherwise usable frame.

The archived binary fixture contains all 8982 original words, FNV-1a
`9b9bfd4a`, under `fixtures/rtos-v0_4_1-bend-Z-ADC12_MS4_POS16.bin`.
The default legacy path rejects it. The opt-in RTOS path decodes Z both in
batch replay and when feeding its retained cadence/projected positions with
the recorded leading quiet duration. Replaying stored inputs is evidence for
this specific rejection fix; it is not a new physical Z acceptance run.

## Targeted change

Only the RTOS target enables `BARCODE_WAVE_RECOVERY_OPTICS=1`. The macro
defaults to zero, retaining legacy acceptance behaviour. The accepted v5.63
release image is preserved, as are its calibration and steering controller.

The new exception requires a motion-domain frame with exactly 31 extrema,
exactly one short BLACK interval between 1000 and 3999 units, and all other
intervals inside the existing bounds. The existing strict Code39 decoder must
validate all 29 runs, both asterisk guards, narrow separators, corrected width
separation and within-class spread. At least two contrast profiles must agree.
Conflicting candidates still reject the frame. Zero/missing edges, short white
gaps, multiple tiny bars and overlong runs are excluded. No width-class or
geometry-fit tolerance was changed, and no expected letter is supplied.

The existing `coherent_mask` diagnostic now also reports profile agreement for
this opt-in strict short-black path; `geometry_mask` retains its earlier meaning.
Motion/line/barcode/MQTT diagnostic packet layouts are unchanged.

## Evidence retention

This diagnostic build retains the first substantial failed optical waveform
for the whole run: at least one second, at least 100 ADC units of contrast,
and up to the complete 8192-reading storage bound. It does not retain paper-only
noise as a useful failure. On stopping, this first failure takes priority in
the waveform export; all attempt summaries still describe the full run,
including subsequent accepted letters. If no such failure exists, the original
latest substantial/accepted snapshot policy applies.

The extra diagnostic cache uses 32768 bytes of raw words plus metadata and
six profiles. Total static BSS is 205868 bytes, compared with 172204 for
v0.4.1. It should be removed or reduced after the full-A issue is resolved,
before final whole-mission RAM optimization. Kernel pool and task stack
requests are unchanged. Static arithmetic headroom is not a runtime stack or
heap measurement. Copying and selecting the cache occur only in the Decoder
task, outside IRQ exclusion; acquisition and the motor-stop guard stay live.

## Next physical check

The configured image is private at
`build/rtos-network-v0_4_2/robocar_rtos_network_v0_4_2.uf2`.
After installation and the stopped connection check, run the same logger:

```sh
sh "/Users/cyix/Github/INF2004-Embedded-Systems-Project/tools/run_rtos_network.sh" --reuse
```

Use normal power and the same full-A / half-A / right-bend half-Z track.
Hold GP28 on the centre tape and GP26 on white for one second; wait for valid
motion data, then press/release GP20. Let GP26 clear the final black stripe
and, if space permits, travel 3–5cm onto white. Press GP20 before handling the
car. Keep the Pico powered until the diagnostic checksum is verified; then
type q and Enter. One run is enough for the next analysis, even if full A
still fails. This build has no sonar/obstacle stop.

Full-A recovery is still a pending physical acceptance item. Terrain/hump,
obstacle manoeuvring and command execution remain with their respective owners.
