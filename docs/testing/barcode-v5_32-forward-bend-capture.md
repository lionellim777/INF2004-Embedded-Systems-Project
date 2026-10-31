# v5.32: motion-aware course capture

## Evidence

Source `docs/testing/logs/20261030-v5_31-bend-A-Z.txt` preserves the supplied12-36-07-183Z download. User placed half A on the straight, half Z at the bend. Latest run: reads=0, edges=64, frames=0, timeouts=0, paused_fragments=2, recoveries=27; final CENTER_LINE_NOT_REACQUIRED. Recovered summary duplicates live result. Neither letter was decoded. Two active fragments were discarded by the phase-level gate; they cannot be labelled A and Z individually from this log. The first optical trace covers first activity only, not guaranteed Z. Actual reverse wheel motion is present in recovery; simply stitching all64 edges would mix recrossed bars.

## Implementation

Centred COURSE_TEST only now stamps each confirmed sensor edge with encoder position in the same1ms ADC callback. First-crossing timestamps and positions are preserved by two-sample confirmation. Position is1000 times the sum of positive new high-water travel of the two forward wheel counts. Raw left encoder forward sign is negative; right positive, as in existing motion control. Unsigned counter wraps are handled. Decode widths use position deltas, while quiet-gap rearm and event timestamps retain wall time. Legacy/stationary diagnostics remain time based. Existing20-bit run storage is retained;750000 units bound one bar/gap to375 mean wheel counts (about27.8mm at nominal2543CPR/60mm wheel geometry).

A stationary stop does not extend the measured width. Both-forward unequal wheel motion is allowed. Stops/confirmation phases no longer unconditionally erase a frame. Up to2s without new wheel progress preserves a partial frame; longer holds discard it. A reverse excursion greater than4 counts on either wheel, or an impossible >64-count change per sampling tick, invalidates queued data across the break and requires a fresh250ms white gap. Backlash excursions within4 counts do not add duplicate travel. Optical edges with zero measured travel discard the frame. Sampling gap/overflow faults remain enforced, and no motor protections or steering gains are relaxed.

Diagnostics: `width_source=ENCODER_AXLE_DISTANCE`, `motion_breaks`, `motion_aborts`, `pause_timeouts`, `zero_distance_aborts`; discard reason plus saved spatial widths. Spatial widths are explicitly labelled `sumcounts_x1000`, never microseconds. No new UI buttons. v5.31 general A-Z decoder with bounded outer-guard tolerance is retained.

## Limits

Axle distance is NOT exact barcode-sensor travel along the printed stripes when rotating. GP26 is ahead and left of the axle. Turning changes its speed, angle and overlap independently of axle progress. This implementation addresses speed changes and stationary pauses on a monotonic forward scan; it cannot recover stripes never sensed or guarantee decoding during a reversing search. It does not estimate barcode orientation, reconstruct a spatial map, or stitch fragments across reversals. Initial reverse excursions below4 counts are tolerated as noise, not proven geometrically harmless. Wheel slip, missed narrow gaps, travel through the printed text, sensor height/contrast and tight-curve projection remain physical risks. This log lacks per-edge encoder positions, so exact distance replay of its failed Z is not possible. Software tests are not proof of a curved-track physical read.

## Validation

Sanitized test suite: all A-Z, two2:1 spatial sizes, both printed orientations, unequal forward wheel rates, per-element speed changes,1s pauses, A/Z sequences,20–100ms foreground batching, reversal/discard/rearm, stationary flicker,2s pause timeout, small encoder bounce, large jumps and counter/measurement wrap. The same variable-speed input fails a time-only decoder and succeeds with distance widths. Synthetic optical signal is based on axle distance; this is not a full sensor-pose model.

Actual firmware main-loop regression:33 centre scenarios (new fixed-spatial Z survives a brief zero-power line confirmation),18 legacy scenarios; prior scenario26's time-changing optical signal during a stop is now correctly rejected as zero-travel activity before a later fresh Y scan. Wireless core/transport/DHCP tests pass with mocked radio. Strict Pico W build and UF2 family/blocks/address/version validation pass. No flashing or physical operation performed. BSS from linked firmware108692 bytes before the final diagnostic-only format change; use size tool for current link. Not a claim about total runtime heap/stack consumption.

## Next physical run

Flashv5.32; same A-then-Z layout, default STRAIGHT course actions, fixed mounting. Expected successful sequence isAZ. Download once after the pass. If Z fails, inspect discard reasons and spatial runs; do not claim that reading while reversing is supported or repeatedly loosen the letter checks. Stop before the table edge.
