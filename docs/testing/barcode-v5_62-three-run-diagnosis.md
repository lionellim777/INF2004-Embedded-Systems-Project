# v5.62: three further physical runs, 6 November

Sources: `logs/20261106-v5_62-evening-run{1,2,3}.txt`, original filenames
15-47-45-641Z /15-49-08-830Z /15-50-30-864Z. Downloads contain historical
telemetry and retained results; use each first current-run barcode/course
result and the ledger matching its capture timestamps. Last stop/result lines
can belong to earlier runs and are not the current outcome.

| Run | Sequence | End | Recoveries | Arcs / rejoins / fallbacks | Confirmation retries |
| --- | --- | --- | --- | --- | --- |
|1|empty|CENTER_LINE_NOT_REACQUIRED|15|2 /1 /1|1|
|2|A|OBSTACLE_STOP|18|1 /0 /1|0|
|3|A|OBSTACLE_STOP|18|2 /1 /1|0|

User reports run1 failed before half-Z, runs2/3 physically traversed it. All
three have zero ADC integrity faults. Sample gaps581/630/603us are healthy;
legacy motion-break counters are not waveform resets.

## Failures distinguished

- Run1 full-A: `MOTION_PATH_INVALID`, one motion fault. The waveform logic
  deliberately invalidates a recross returning more than3mm sideways from its
  previous frontier. This is a provisional software geometry restriction,
  not proof that the ink was unreadable. Usable barcode height and actual
  optical coverage are needed to select a justified tolerance.
- Run1 half-A: frozen waveform3930 words, FNV8026269c. At48ADC prominence the
  reader finds31 extrema, but rejects an estimated7.346-count stripe before
  Code39 classification because the absolute minimum is8mean-count units.
  Higher prominence loses faint features; lower prominence adds wiggles.
- Run2 full-A accepted. Half-A ledger has no inspected widths and zero pattern
  masks, consistent with rejection before character classification. Its full
  waveform is replaced by the later Z snapshot, so the precise earlier
  rejection cannot be established from this export.
- Run2 Z: frozen5764 words, FNV204f0039. High-prominence profiles give complete
  094/0d0/094 guard/payload ranks. Both guards fail narrow-class spread. The
  scan has89mm lateral excursion/24deg yaw; fixed-axis projection and optical
  geometry may contribute, but the log cannot isolate their physical cause.
- Run3 full-A accepted. Half-A is rejected, with no detailed candidate widths
  retained. Its individual raw waveform is unavailable, so its precise
  rejection is unconfirmed.
- Run3 Z: frozen5270 words, FNVf170f57c. Several profiles give31 extrema but
  are blocked by a7.427-count estimated stripe versus the8-count minimum.

The8-count floor corresponds to about0.593mm under nominal wheel geometry;
7.346/7.427 counts correspond to approximately0.545/0.551mm. These are reader-
estimated apparent widths, not measurements of the physical printed bars.
Optical crossings, skew and motion projection can change apparent widths.

## Read-only replay experiment

All three exported waveforms pass checksum validation and remain rejected by
unchanged v5.62. A temporary host-only decoder with the absolute floor reduced
from8000 to4000units recovers only run3's Z, with the rest of the width/model
checks intact. Run1 half-A and run2 Z remain rejected. This experiment does not
justify publishing a one-threshold fix and is not a physical read. Firmware
source and the released v5.62 image are unchanged in this investigation.

The confirmation retry fix executed in run1; its final failure is exhausted
line reacquisition rather than the former confirmation dead end. Arc rearming
also occurs now. Thus increasing retry time alone is not a demonstrated fix.

## Information requested

Confirm whether “half” means the whole symbol scaled smaller, reduced bar
height, or a cropped symbol. Measure narrow/wide BLACK widths, narrow/wide
WHITE widths and bar height, and identify which print the previous3mm black
measurement refers to. Confirm flat rectangular versus curve-following Z
layout, route tape width and GP27-to-GP28 lateral sensing-spot spacing.
The prior repository note `barcode-print-sizing.md` describes a supplied PDF
and a separately generated practice sheet; neither proves the current print
scale or print source. Do not assume the old nominal3:1 geometry is current.

A top-down photo with a ruler can additionally check sensor-path/ink alignment.
The next changes should use measured geometry, retain failed short-run widths,
and isolate a straight half-A capture before relying on bend compensation.
No new firmware was produced solely from this replay experiment. Further
parameter selection awaits the requested physical dimensions.
