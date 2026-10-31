# v5.34: independent optical barcode capture and bounded recovery assist

## Physical evidence

User confirms neither v5.33 run involved touching or lifting the car. Archived inputs:
- `logs/20261030-v5_33-independent-input-run1.txt`
- `logs/20261030-v5_33-independent-input-cumulative-runs1-2.txt` (contains run 1 again; not a third run).

Run 1: no accepted letters, 46 optical edges, 1 timeout, maximum sampling interval 1040 us, no sampling integrity fault. Final stop `CENTER_SEARCH_ENCODER_STALL`: both encoder counts remain nearly stationary while opposite 18% PWM is requested. 24 recoveries and 35 trims. This is real non-response reported by encoders, not assumed handling.

Run 2: no accepted letters, 20 optical edges, 2 timeouts, maximum sampling interval 1052 us, no sampling integrity fault, 1 motion-triggered capture abort. Final stop `CENTER_LINE_NOT_REACQUIRED`, 25 recoveries and 31 trims. The first capture times out before recovery starts, so recovery coupling does not explain all missed bars. Neither final stop is a sonar stop.

## Changes

- ADC timer 1000 -> 2000 samples/s (500 us period). Same single IRQ owner for all ADC channels; no decoding or printing in the IRQ. Foreground consumes timestamped transitions while navigation runs.
- GP26 mean filter: 10 samples at 1 kHz (10 ms) -> 12 samples at 2 kHz (6 ms). Entry/release thresholds remain 226/220. Two-sample confirmation now needs consecutive 500 us samples. Trials with 3 ms and 4 ms filtering failed a retained low-contrast reconstructed fixture; 6 ms passed. This fixture is reconstructed from aggregate bins, not a complete raw physical replay.
- Centred course capture uses optical elapsed-time widths, not axle displacement. Steering phase and reverse encoder metadata no longer reset optical capture. Motion metadata remains diagnostic. Optional legacy distance capture and tests remain.
- A failed 29-interval candidate slides by two intervals to search the next black-aligned complete frame. A complete `*letter*` frame and existing width/guard validation remain mandatory. Partial frames and arbitrary noise do not become A/Z guesses. After a valid read, 250 ms white rearm remains. Overlong intervals still expire; arbitrary stops/backtracking or a physically missed stripe cannot be guaranteed to decode.
- Y/Z action dispatch remains separate: if recovery/turning is underway, the decoded action is deferred; an explicit mapped stop still stops. A is recorded but has no Y/Z mission mapping.
- Recovery pivot starts at 18% duty. A wheel without progress for 150 ms receives a bounded 0.20-duty-per-second increase, capped at 28%; detected progress restores 18%. Existing sweep/time/1-second stall limits remain. This is a provisional motor-response change, not a diagnosis of the motor or wiring.
- Result logs add ADC execution duration, actual sample-gap evidence and 500 us/12-sample settings. UI no longer incorrectly states decoding pauses during recovery.

## Reference implementation reviewed

Gerald's [barcode scanner](https://github.com/Gerald-codes/Line-Following-Robot/blob/main/src/hardware_drivers/barcode_scanner.c) timestamps digital GPIO edges, records interval widths, and classifies narrow/wide Code 39 elements outside the edge callback. It selects the first alphanumeric decoded chunk without requiring both complete framing guards. Our analogue implementation uses the same separation principle while retaining complete framed-letter validation; it does not copy their pin setup, threshold hardware, ISR logging or weaker frame acceptance.

Their [line follower](https://github.com/Gerald-codes/Line-Following-Robot/blob/main/src/control_algo/line_following.c) also increases recovery effort and recentres after finding the line. This source is a reference, not proof that their hardware or trajectory matches ours. Main integration file downloaded empty; no assertion about their full scheduling architecture is made.

## Verification

- Sanitized barcode/motion suite passes. New optical tests cover A-Z, both directions, 15/30/60 ms narrow widths, junk prefixes, contradictory encoder metadata, AZ sequencing, incomplete frames, equal-width noise, timeout recovery and timestamp wrap.
- Recovery tests exercise stuck wheels, independent wheel assist, effort cap, reset on progress, and unchanged stall deadline.
- 34 centred main scenarios pass, including production `barcode_drain` held in LOST/TURN/SEEK/BRAKE with valid Z accepted and its action deferred. This tests software phase independence, not whether a real pivot crosses stripes in a decodable order.
- 18 legacy main scenarios pass; mocked wireless core/HTTP/transport tests and actual lwIP DHCP checks pass.
- Strict Pico W build passes; all UF2 blocks, family, addresses, version and embedded UI checked. Linked text 461880 B; BSS 108704 B (not total runtime RAM consumption).
- Build report: `build/motion-wireless/build-report-v5_34.json`.

No hardware operated or flashed. Actual 2 kHz timing, optical contrast and moving half-A/Z acceptance remain unverified. Sampling faster cannot separate overlapping black/white levels; further results must be judged from accepted letters and integrity/timing evidence, not the visual traversal alone.

## Next physical run

Flash v5.34, refresh the page, use the same centred GP28 arrangement and course mode with both actions straight. Run the existing A then bend/Z route once and download the log. Inspect accepted sequence, sample gaps/ADC duration, recovery count and actual stop reason. No sensor-height or paper-position change is requested for this comparison.
