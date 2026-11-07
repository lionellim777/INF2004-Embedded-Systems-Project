# v5.58: resynchronising waveform scan and complete failure export

## Observed v5.57 failure

Source: `logs/20261105-v5_57-half-Z-rejected.txt`, copied without alteration from
the user's 5 November 2026 course-mode download. The user reports traversing
half-Z. Recovered results and live snapshots repeat the same course run; they
are not separate passes. Earlier obstacle-mode results also appear in the file.

The course run has `reader=LOCAL_EXTREMA`, zero reads, one rejected capture,
36 legacy fixed-threshold edges, no sampling integrity fault, max ADC gap 543 us
and ADC work up to 246 us. These edges are not the new reader's peak sequence.
The final stop is `CENTER_LOCAL_RECOVERY_EXHAUSTED`, not a barcode action.

Countdown GP26 readings are approximately 322..351. Moving readings later fall
into the 220..255 range, and capture is already active before the larger logged
pulses. The reported ADC contrast reaches 168..4084. This shows a changing
background and capture activity, not a complete measured stripe history. The
file has log gaps and no optical-bin lines or waveform profile details. It
cannot identify the exact failed bar, guard or width comparison.

## Confirmed software issues and changes

1. **Lost diagnostics:** the v5.57 retained-result collector cleared its waveform
   field on `[OPTICAL START]`. Production emits a retrospective `source=DRIVE`
   optical dump after the waveform result, so it erased the new diagnostic line.
   v5.58 clears the waveform field when a new motion begins/stops, and preserves
   it across the later plot header. A regression covers this exact order.
2. **Whole-recording rejection:** v5.57 requires exactly 31 extrema for the whole
   recorded waveform. Extra prefix bumps reject a genuine complete framed code.
   v5.58 searches black-aligned 29-interval windows in up to 95 extrema. It does
   not insert/remove interior stripes or widen Code39 acceptance thresholds.
   All successful windows/profiles must agree on one letter. Contradictory
   letters reject. This is a plausible failure mechanism for the physical run,
   not a proven reconstruction of its missing samples.
3. **Incomplete failure evidence:** v5.58 freezes the latest accepted capture,
   or otherwise the longest completed/stopped-partial capture. The same filtered
   2 ms ADC inputs used by the decoder, their stored elapsed times and all six
   prominence profiles survive until the next motion start or power reset.

The new prefix regression is 52 faint A-Z/both-direction cases. Every prominence
profile has an extrema count other than 31, so v5.57's whole-recording gate
rejects these constructions; v5.58 finds their intact framed windows. One full
production-main scenario exercises faint Z after two prefix bumps through the
IRQ queue, reader, event dispatch and frozen-export publication.

## What Download log now includes

After the car stops, the existing button automatically reads the immutable
snapshot from `/waveform?offset=N`, in chunks of at most 256 readings. No motion
is started by this endpoint. Publication is protected by the existing cross-core
mutex. The owner unpublishes before starting/reusing recording storage; an
interrupted download cannot silently mix captures.

The downloaded text has:

- `[WAVEFORM SNAPSHOT]`: capture ID/start time, reading count, white baseline,
  leading/trailing white durations, total recording time, saved-result reason
  and letter, checksum and snapshot-selection rule.
- Six `[WAVEFORM PROFILE]` lines: prominence, extrema, candidate windows, best
  window, rejection category, offending run index/width where applicable,
  direction, symbol masks, strict width-failure names, guards/separators and
  the best candidate's 29 widths. Strict width diagnostics do not themselves
  include the existing bounded outer-bar fallback; accepted status remains
  determined by the unchanged production decoder.
- `[WAVEFORM CHUNK]`: offset/count/total/capture ID and packed 16-bit readings.
  Each four-digit hex word stores ADC in bits 0..11 and elapsed milliseconds
  in bits 12..15. The first word is the sample before the trigger (time zero).
  Sum later stored steps to reconstruct the decoder's waveform timeline.
- `[WAVEFORM COMPLETE] ... checksum=OK` only after every reading is collected
  and its FNV-1a checksum verified. Hash input is each word's low byte then high
  byte. This is a completeness check, not a cryptographic integrity claim.
- `[WAVEFORM EXPORT INCOMPLETE]` if a chunk is missing/corrupt, the capture
  changes, a request fails or the car resets. The page explicitly says to
  download again before another run. No frozen capture returns an explicit
  unavailable marker rather than a success marker.

The original 16 KiB live-log ring and five-second optical-bin plot remain
bounded and can have gaps. Frozen export bypasses that ring. It contains
filtered 2 ms decoder inputs, not all unfiltered 500 us ADC samples. Time steps
retain the existing 1 ms storage quantisation. Actual target decode work and
moving optical resolution still require hardware evidence.

## Validation

- Existing 832 waveform matrix cases and 3000 noisy +/-15% width-jitter cases
  pass; no wrong letters in that finite corpus.
- 52 new prefix cases, two conflicting framed letters, short/incomplete frames,
  stopped-partial storage, exact frozen-sample checksum and detail formatting.
- 51 centred and 18 legacy production-main scenarios, including new prefix-Z
  integration, snapshot publication, fault/no-action cases and retained motion
  stops. Existing 15 barcode/motion suites and packed-storage/ARM layout checks.
- Actual HTTP handler: first/final chunks, bounds/malformed offsets, metadata,
  unpublication and twelve maximum retained result lines.
- Mocked browser: two-chunk complete download, checksum corruption, capture-ID
  changes, explicit incomplete markers, standard UI controls/download/reset and
  mobile layout. Radio/car responses are mocked.
- Strict Pico W build and UF2/embedded-page verification: see
  `firmware/releases/build-report-v5_58.json` for final sizes/hash.

No robot was flashed or operated by the agent. The actual v5.57 failed stripe
sequence remains unknown; software regression recovery is not physical success.

## Next physical pass

Flash `firmware/releases/robocar_motion_integration_v5_58_wireless.uf2`. Reconnect
to AAI10-Calibrate and reload the page. Verify v5.58. The page now initially
selects **Follow track + read barcodes**; still click Prepare run before GP20.
Keep both Y and Z straight for this comparison.

Keep the current approximately 1 mm clearance and 3 mm narrow bars unchanged.
Start GP26 over 30 mm of plain white, with GP28 on the tape. Traverse the whole
half-Z and at least 30 mm of white after it; keep tape under GP28 through that
exit if the short track permits. Stop with GP20, then click Download log once.
Wait for collection to finish. Do not start another run or reset before download.

Send that single text file whether Z is accepted or rejected. The success case
is `letter=Z`, `reads=1 sequence=Z`, and no integrity fault. A failed scan should
now include `[WAVEFORM COMPLETE] ... checksum=OK` and its precise profile/width
evidence, removing the need to infer stripes from low-rate status lines.
