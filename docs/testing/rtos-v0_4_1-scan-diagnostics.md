# v0.4.1 wireless scan diagnostics

The first physical v0.4 RTOS run confirmed A at 101868ms and A at 111876ms,
but no Z. Retained USB counters showed 17520 retained samples, 7 frame checks,
2 rejected captures, 2 accepted reads and 33 course recoveries. Capture faults
were zero. Those aggregate counters cannot assign a rejection to Z or explain
its cause. Sparse wireless ADC readings showed white while the car continued
moving after the third-barcode region, making an early stop less likely without
proving that the complete printed Z was traversed.

v0.4.1 adds evidence collection. It does not retune or replace v5.63's optical
decoder, course following, motor PI control or sensor geometry. Hump detection,
obstacle manoeuvring and barcode command execution remain separate work.

## Operator check

The configured v0.4.1 image is private under
`build/rtos-network-v0_4_1/robocar_rtos_network_v0_4_1.uf2`. The v0.4 image and
accepted v5.63 release remain intact. After installation, use the same command:

```sh
sh "/Users/cyix/Github/INF2004-Embedded-Systems-Project/tools/run_rtos_network.sh" --reuse
```

1. Use the same full-A / half-A / right-bend half-Z track and normal power.
   The Mac can remain on the home router's 5GHz network; the Pico uses 2.4GHz.
2. Put GP28 on the centre tape, GP27 on white to its right, GP26 on white
   before the first barcode. Hold still for one second. Wait for valid MQTT
   motion data, then press/release GP20 once. No remote command starts it.
3. Complete one run. If track space permits, let GP26 clear the last black
   stripe and travel another 3–5cm on white before pressing GP20 to stop.
   Stop sooner if necessary; the new capture will identify a stopped partial
   scan. This build has no sonar stop; use GP20 before handling the car.
4. Keep the Pico powered and leave the logger open. After stopping, it prints
   each capture result and then `Scan diagnostics saved; waveform checksum
   verified.` The waveform downloads only while scanning is stopped.
5. Type `q`, Enter after that message. If q is entered sooner, the helper asks
   for stops and waits up to 45 seconds for the download. It explicitly warns
   if the capture remains incomplete. Do not switch power off during download.

Files are saved automatically in the new session under `serial-output/`:
`mqtt.jsonl`, `mqtt-summary.json`, `scan-<boot>-<run>.json` and, when complete,
`scan-<boot>-<run>-waveform.txt`. The shared folder is directly accessible to
the agent; manual uploads are unnecessary. USB can be unplugged for the run.

## What is collected

The diagnostic state adds scanner counters, queue high water, timing/queue
fault bits, course recovery count, decoder and ADC service maxima, scan enable,
recording/sealed flags, stop timestamp and the number of queued samples discarded
at stop. Service maxima are elapsed observations, not established deadlines.

Every stored attempt (up to the existing 32-attempt bound) includes the result,
captured letter, time, sample count, optical range, estimated yaw/lateral travel,
profile-consensus masks, best profile, Code39 guard/pattern/width diagnostics and
29 run widths. The overflow count makes missing later attempts visible. These
are decoder/odometry observations, not proof of physical path or barcode identity.

After stopping, the Decoder task saves an in-progress scan as `STOPPED_PARTIAL`
without accepting it as a barcode. Motors have already been disabled. The
existing stop path discards queued samples; the new counter reports how many.
The snapshot selection remains the original latest substantial/accepted capture,
which may differ from the final short/noisy attempt. All available attempt
summaries are exported alongside it. Only this one selected waveform is stored.

## Binary transport and isolation

New topic: `inf2004/aai10/robot1/v1/scan_diag`. Existing motion/line/barcode
schemas are unchanged. The new topic is added to the authenticated robot's
publish ACL; observer credentials still cannot forge robot telemetry.

All records start with kind8 and run-token16, use least-significant-bit-first
fields, and validate final-byte zero padding. Run tokens are scoped by the
status boot ID. New runs cannot export data from an old generation. Native
data is copied under short IRQ exclusion; binary encoding and networking occur
after restoring interrupts. No decoding or networking is added to the ADC IRQ.

| Kind | Bytes | Fields following the common header |
| --- | ---: | --- |
| 0 state | 46 | ms/samples/frames/rejected/decode-us/ADC-us: 32 each; reads16; queue9/faults3/enabled1/recording1/sealed1/pad1; attempts6/ready1/pad1; overflow/recoveries/stop-us:32 each; discarded9; pad7 |
| 1 attempt | 153 | index5; start32/duration32/samples14; min12/max12/lateral16/yaw16/reason4/letter5/coherent6/geometry6; profile; pad7 |
| 2 snapshot | 31 | word-count15/motion1/paper12/range12/reason4/letter5/coherent6/geometry6; start/leading/white/duration/checksum:32 each; pad3 |
| 3 profile | 133 | index3; profile; pad4 |
| 4 words | 7–133 | chunk-index8/count7/pad1; count words of 16 bits; max64 words |
| 5 done | 4 | attempt-count6/snapshot-present1/pad1 |

Profile fields: extrema7/windows6/best-window6/reason4/bad-run5/bad-width32;
three Code39 patterns9 and three width reasons3; guards2/separators2/reversed1/
score4; 29 run widths32. Reason4 and letter5 encode the existing enums/A–Z.
Snapshots retain the original ADC12/elapsed-ms4 words, interleaved signed
position16 in motion mode. The FNV-1a checksum covers little-endian word bytes.
Counts and widths retain their original domains; these are not pixel lengths.

State is published with the existing one-second status service. After a run
is sealed, bounded diagnostic packets are sent with one QoS1 request in flight.
The cursor advances only after broker acknowledgement. Backpressure retries the
same record; reconnect replays the export. The PC handles duplicates/reordering,
verifies all profiles/chunks/attempts and the checksum, and reports completion.
Data is not persistent across a Pico power cycle or a subsequent scan start.

The text waveform export is compatible with `tools/replay_waveform_log.py`.
Batch replay can explain a pattern rejection but does not prove valid moving
framing, quiet zones, slip-free odometry or physical track acceptance.

## Validation boundary

The sanitized host integration exercises the real ADC callback, queue, course,
decoder, motor gate and export, including A/A/Z, recovery, stop-partial and run
isolation. Independent Python decoding verifies every exported field, all six
profiles and the waveform checksum. The full 16384-word/256-chunk bound, missing
or damaged chunks, duplicates, ordering, truncation and overflow rejection are
covered. MQTT tests cover actual service ACK ordering, retry, reconnect and old
run ACK isolation, plus real local broker delivery and authenticated ACLs.

These tests do not establish a physical Z fix. The next one-run capture is
needed before changing the controller or decoder based on evidence.
