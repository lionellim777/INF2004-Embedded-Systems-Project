# v5.33: measured wheel-difference steering

## Physical evidence entering this revision

Archived cumulative v5.32 logs: `logs/20261030-v5_32-early-steering-stops.txt` and `logs/20261030-v5_32-A-bend-Z.txt`. The second contains all three runs; its recovered result duplicates the last run.

- Runs 1/2 ended with CENTER_STEERING_NO_RESPONSE at counts929/919 and647/645, before any barcode edges. Both wheels moved. Run1 requested left targets around515/600cps while sampled actual rates were513/513, then516/562. Failing the20 differential-count response test is not equivalent to an encoder stall.
- Run3 accepted A at4933ms, no Z: reads1 sequenceA. It logged25 recoveries and33 trim attempts. A later partial capture was discarded for REVERSE_OR_ENCODER_JUMP; the final stop was CENTER_LINE_NOT_REACQUIRED. No claim of successful bend-Z scanning.

## Changes

The centre-course drive now closes a wheel-difference travel loop in addition to each wheel's speed PI. Every fresh20ms encoder interval, it accumulates requested minus measured left-minus-right travel, clamped to40 counts either way. Differential duty is0.0012 times that error plus0.00004 times differential speed error, capped at0.06 duty each side. Both final forward outputs stay between0 and0.50. This also corrects straight travel; the centre profile no longer uses the separate legacy straight-position target trim. Legacy straddling and calibrated30cm/90degree/180degree manoeuvres retain their controllers.

The startup residual wheel skew initializes the differential error. When optical trim reverses or returns to strong tape, unfinished differential demand is cleared so an obsolete turn is not completed after the sensor says to stop correcting. New segments reset the differential loop along with speed history, while retaining the existing bounded launch effort.

After1s without20 differential counts in the requested direction, optical trim still expires, but the course brakes into its existing bounded recovery instead of immediately latching CENTER_STEERING_NO_RESPONSE. Genuine wheel-stall, control-delay, search travel/time, invalid encoder and obstacle protections remain. This fallback can still stop if it cannot find the line. It does not make arbitrary searching safe or guarantee alignment from one black reading.

Logs add TrimNoResponse, YawError and YawPWM; the final course result adds trim_no_response. No new UI controls. Barcode decoder, thresholds, distance capture and reversal rejection remain asv5.32. A reverse sweep can still discard a partial barcode. Axle travel is still an approximation to the offset barcode sensor's path on curves.

## Verification

- Sanitized barcode/motion regression suite passes, including distance-width A-Z/size/direction, reverse/zero-motion rejection, existing decoder fixtures and encoder/motor guards.
- New unequal-friction simulated wheel tests exercise left/right75cps and220cps differential requests, both friction orderings, return to straight, forward-only bounded outputs and bounded error. All obtain20 counts in the requested direction within1s. Gentle requests produce57.8–94.2 differential counts after1s in this simple synthetic plant. These are software results, not a measured motor model or physical turn-angle accuracy.
- New course regression supplies weak-but-visible tape and equal forward wheel progress for1s: it brakes into COURSE_LOST, records one failed trim response, and resumes only after strong-black stopped confirmation. Existing bounded-search failure tests still pass.
- Mocked wireless core, HTTP fragmentation/pressure/lifecycle and DHCP regression passes; no radio hardware operated.
-33 centre and18 legacy production-main scenarios pass. Strict Pico W build passes. UF2 block/index/address/family/version checks pass; old terminal steering-stop marker absent.
- Firmware: `../../build/motion-wireless/robocar_motion_integration_v5_33_wireless.uf2`;923136bytes/1803blocks. SHA256 `f017623ae6247ad28ee4b07611ad36e7325835c44034a4d095af41da81af116d`.
- Linked text461400bytes, BSS108700bytes. This is not total runtime stack/heap use.

## Next physical test

Flashv5.33, reconnect and refresh the existing page. Keep default straight course actions and the same full/half-A then bend/half-Z arrangement with unchanged sensor mounting. Prepare the course run and pressGP20. One pass is enough for the next diagnosis: observe early stop/recovery, straight drift and whether the robot crosses both prints. Download the log. Acceptance target is reads2 sequenceAZ with fewer recovery sweeps; neither is claimed achieved yet. Stop before the table edge.
