# v5.61: motion-aware GP26 waveform decoding

## What the three v5.60 runs actually show

Read the **first current-run result** in each export. The downloads also contain
older runs and repeated retained snapshots; those are not additional attempts.
Original exports are archived as `logs/20261106-v5_60-barcode-run{1,2,3}.txt`.

| Run / download time | Accepted sequence | Rejected captures | Recoveries | Trims | Sampling fault |
| --- | --- | --- | --- | --- | --- |
| 02:33:24 | empty | 5 | 23 | 30 | none |
| 02:34:43 | A | 3 | 22 | 63 | none |
| 02:37:25 | empty | 7 | 22 | 42 | none |

The second run accepted A at8323ms. With the reported track order, this is the
first/full A. Neither half-A nor half-Z was accepted in these three runs.
All three ended with `OBSTACLE_STOP`. The user reported traversing the bend;
logs show zero confidence-gated rolling-arc attempts, so this traversal used
normal trim/search recovery. It is not evidence that the timed GP27 arc fired.

The timer kept sampling: maximum sample gaps615/623/580us, zero waveform queue
overflows and zero waveform timing faults. Legacy `motion_breaks`293/226/285
are diagnostic counters from a different acquisition path; they did not reset
this waveform reader. Line seeking did not disable the GP26 timer.

The recorded reader used **time widths** (`width_source=TIME`). Stops, uneven
speed and recrosses change these durations even when printed stripe widths
are constant. v5.60 changed the movement pattern while retaining that reader.
This is an identified algorithm limitation, not proof that every rejection
was caused by a stop.

The frozen captures verify FNV checksums2842d079/a43348f0/d063d8d5. All three
remain rejected in time-domain replay. Sweeping prominence4..199 also finds
no strictly valid frame. Low-prominence profiles include optical wiggles;
higher profiles lose faint narrow bars. The third recording has a24-ADC
candidate whose reversed rank masks are094/0d0/094 (`*Z*`), but all three
characters fail the existing wide/narrow separation requirement. These ranks
are evidence of a plausible Z, not an accepted decode. Raising contrast or
loosening width checks alone is not a supported fix.

Only one detailed waveform was retained per run. The later bend capture can
replace the earlier half-A failure, so its precise optical/motion failure
cannot be reconstructed from these files. No synchronized per-sample wheel
positions exist in them; a spatial replay of these old runs would invent data.

## Change

- Keep v5.60 course steering, drift thresholds, timed GP27 arc confidence gate,
  bounded recovery and obstacle controls. No v5.61 steering-policy changes.
- Copy both raw wheel counters beside every500Hz filtered GP26 sample in the
  existing interrupt queue. Pose computation and decoding run in foreground;
  the2kHz sensor timer keeps running during LOST/TURN/SEEK/BRAKE.
- Use the user's measured GP26 position: **80mm forward,85mm left of the
  powered-wheel midpoint**, diagonal116.73mm. The front caster is not the axle
  reference. This changes GP26 barcode geometry only, not GP28/obstacle lead.
- Integrate differential-wheel pose using the existing nominal60mm wheels,
  2543counts/revolution and115mm track. Compute the offset GP26 sensing spot,
  then project it along the car heading at capture start. Width units are1000
  per mean-wheel count; one mean count is approximately0.07412mm.
- Extract prominent optical transitions from the forward spatial envelope.
  Stops and samples behind the furthest observed position add no width or
  duplicated stripes. A recross that resumes more than3mm sideways from its
  former frontier invalidates that frame (`MOTION_PATH_INVALID`). It does not
  stop tracking, and a later fresh frame can be acquired.
- Retain the complete Code39 guards,29 intervals, separators, existing width
  separation/spread bounds, forward/reverse symbol checks and conflicting
  candidate rejection. No expected A/Z hint, inserted stripes or relaxed width
  tolerances. A whole backwards-driving scan is outside this forward-envelope
  reader; reverse symbol orientation while driving forward is supported.
- A trailing white region needs250ms **and100 new projected counts** before a
  decode attempt. Failed captures abandon after750ms and300 new white counts.
  A stationary/repeated white gap cannot alone end the recording. The16s frame,
  8192-reading buffer, ADC/queue timing and motor service guards remain bounded.
- Keep up to32 independent attempt summaries until the next run: timestamps,
  duration, raw range, lateral/yaw excursion, best profile/width rejection,
  symbol masks and all29 widths. Print these after motor stop. Overflow is
  explicit. Short captures now have their own record even if the latest full
  waveform remains a more useful older capture.
- Frozen exports interleave ADC12_MS4 and signed16-bit projected position words,
  with `encoding=ADC12_MS4_POS16`, `readings` and `width_domain`. `samples` counts
  exported16-bit words (up to16384). FNV covers both words. The page downloads
  up to64 chunks and still rejects incomplete, changed or corrupt snapshots.
  The latest substantial waveform is still the only full waveform retained;
  the attempt ledger does not promise full raw samples for every frame.

## Validation and limits

Host checks pass with address/undefined-behaviour sanitizers:52 A-Z/orientation
spatial frames with4x speed changes,600ms interior hold and same-path recross;
52 analytic right-bend cases with the measured offset; repeated A/Z frames;
frozen interleaved replay; encoder-jump and lateral-path mismatch rejection;
existing832 contrast/size waveform cases,3000 noisy cases and negative framing
cases. Actual main-loop tests cover53 centred and18 legacy scenarios, including
scanning/deferring actions in LOST/TURN/SEEK/BRAKE and during a right arc.
Controller, obstacle, existing barcode/motion, storage and wireless tests pass.
Mock Chrome verifies mobile/control behaviour and a9000-word/36-chunk download,
checksum failure and capture changes. RP2040 Release builds with warnings as
errors. See the versioned JSON for artifact hash, memory and UF2 checks.

The five archived previously accepted A snapshots still replay as A. The archived
`fixtures/v5_58-half-Z-ADC12_MS4.bin` decoder-input binary (2202 words,
FNVbec1cc17) still replays Z;
its original Downloads text file is no longer present. All three v5.60 rejected
waveforms remain rejected without missing odometry. These are host checks, not
physical v5.61 acceptance.

A fixed projection assumes a printed barcode whose stripes share an axis close
to the initial car heading. Large skew, wheel slip, changed mounting, curved or
warped printing, missed stripes, an off-paper path or a materially different
recross can remain undecodable. Sensor sampling and steering can run together
on this hardware; arbitrary motion cannot guarantee a recoverable optical path.
The3mm rejoin bound and wheel geometry are provisional, not calibrated optical
coverage. Physical CPU work/stop clearance and A/A/Z consistency still need a
robot run. The new log records max_work_us, holds, recrosses and motion faults.

## Next physical run

Flash `firmware/releases/robocar_motion_integration_v5_61_wireless.uf2` using
BOOTSEL. Check the page/log saysv5.61. Select **Follow track + read barcodes**,
leave both Y/Z actions **Straight**, prepare and start with GP20. Keep the same
sensor mounting and full-A -> half-A -> right-bend half-Z track. Stop after
clearing the last barcode, leaving some white paper beyond it. Download the
log **before starting another run**. Target accepted sequence: `AAZ`.

One run is enough for the next diagnosis. Send its complete downloaded text;
it should include `[SCAN ATTEMPT]`, `[SCAN WIDTHS]`, `[WAVEFORM MOTION]` and
`[WAVEFORM COMPLETE] ... checksum=OK`. Do not infer success from peaks or
plausible symbol masks: use the accepted barcode events/result sequence.
