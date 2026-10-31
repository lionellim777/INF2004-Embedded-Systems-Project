## 30 October 2026 — v5.34 independent optical course capture

Both untouched v5.33 runs accepted no letters; final stops were recovery encoder stall and line not reacquired. v5.34 doubles ADC sampling to 2 kHz, shortens GP26 smoothing from 10 ms to 6 ms, removes centred-course encoder/phase capture resets, and searches successive complete framed-letter candidates. Recovery adds bounded per-wheel effort when encoders show no progress. See [evidence, reference comparison and limitations](testing/barcode-v5_34-independent-optical.md). Sanitized suite,34 centred and18 legacy main scenarios, wireless/actual-lwIP checks, strict Pico W build and UF2 verification pass. Physical validation remains pending; no car operated or flashed. UF2: `build/motion-wireless/robocar_motion_integration_v5_34_wireless.uf2` (924160 bytes). SHA256 `3f12def79c2961d4438417d3e90c1e6a0454f6cfced7d9115104afce3abb3568`. Next: same A then bend/Z course, actions straight, inspect decoded sequence and sampling/recovery results.

## 30 October 2026 — v5.33 wheel-difference steering and recoverable trim timeout

Physical v5.32: two early CENTER_STEERING_NO_RESPONSE stops before any barcode; third run read only A, with25 recoveries and no Z. v5.33 adds bounded20ms differential-travel feedback to the centre drive alongside speed PI, clears obsolete turn demand when the optical request changes, and routes unsuccessful gentle trims into existing bounded recovery rather than immediately ending the run. Actual stall and other guards remain. Barcode decoding/capture unchanged. Diagnostics now expose differential error/duty and trim-response fallbacks.

Sanitized regressions, asymmetric wheel-response simulations,33 centre and18 legacy main scenarios, strict Pico W build and UF2 verification pass. No physical acceptance claimed. See [evidence and test plan](testing/steering-v5_33-wheel-difference.md). UF2: build/motion-wireless/robocar_motion_integration_v5_33_wireless.uf2; SHA256 f017623ae6247ad28ee4b07611ad36e7325835c44034a4d095af41da81af116d. Next: same A then bend/Z route, default straight actions, inspect startup and recovery count as well as accepted sequence.

## 30 October 2026 — v5.32 motion-aware forward barcode capture

Latest challenge log (`docs/testing/logs/20261030-v5_31-bend-A-Z.txt`) decoded neither A nor Z:64 edges,2 phase-discarded partial frames,27 recoveries. v5.32 uses encoder axle-distance widths for centred course scans, preserving frames through forward steering and brief stationary confirmations. Actual >4-count reverse excursions or encoder jumps discard partial scans;2s no-progress and zero-travel-edge guards remain explicit. New logs distinguish these causes. It does NOT reconstruct missing stripes or promise a read while the offset sensor pivots/backtracks across a barcode. See `docs/testing/barcode-v5_32-forward-bend-capture.md`.

Sanitized barcode/motion suite,33 centre-main and18 legacy-main scenarios, mocked wireless core/transport/DHCP checks, strict Pico W build and UF2 verification pass. New spatial Z main-loop case survives zero-power line confirmation. Real curved-track A/Z validation remains pending. No motors operated or firmware flashed. Artifact `build/motion-wireless/robocar_motion_integration_v5_32_wireless.uf2` (919552 bytes), SHA256 7b70ac99305837e079be3d16304d6c0baccb2c206d9e776522d836a423f2b16e.

## 30 October 2026 — v5.31 bounded outer barcode marker tolerance

The latest v5.30 half-A pass captured29 runs with no sampling fault or recovery-discarded fragments, but rejected its end marker: final narrow bar125ms, neighbouring narrow black bars about41–42ms. v5.31 retains strict payload/marker patterns and adds bounded tolerance for ONE expanded physical outer narrow guard bar; it neither guesses A nor repairs missing edges. Bounds: <=3x other corrected narrow median and <=80% of shortest corrected wide run; other elements remain strict. See `docs/testing/barcode-v5_31-half-A-marker.md` for evidence, limitations and tests. Steering and GP26 thresholds remain v5.30 settings.

Exact logged-edge replay now produces A, including through continuous capture. Sanitized barcode/motion regressions, all supported letters/directions and boundary rejection cases pass.32 centre-main and18 legacy-main scenarios and strict Pico W build pass. Physical half-A decoding remains pending. Artifact: `build/motion-wireless/robocar_motion_integration_v5_31_wireless.uf2` (913408 bytes; SHA256 47312c24ae3a2bdbdb6ec6b8ff2093de89d59834a53665f1fcdc7661c5603662). No flashing or car operation performed.

## 30 October 2026 — v5.27 provisional filtered GP26 threshold trial

Six labelled optical captures resolve why a faster poll alone will not help: current251/220 detects mostly wide bars in the full half-A trace; the narrow peaks are near the white threshold. The last capture supports a low-contrast trial, while earlier sweeps have inconsistent contrast and unknown height/angle. See docs/testing/barcode-v5_27-half-A-calibration.md for source windows, deductions, reconstruction limitations and test scope.

Moving GP26 now uses a10-sample mean at1 ms, black entry226 / white release220; legacy stationary sampler defaults stay251/220. Raw optical diagnostics, line-following settings and strict general A–Z decoding remain. Four bounded-window reconstructions recover A with the real filter/sampler/decoder; partial inputs yield no letter. Filtered A–Z synthetic matrix at30/60 ms narrow widths, prior barcode/motion regressions and32 centred main scenarios pass. This is a provisional hardware-calibrated profile, not established moving half-A acceptance or a fix for every capture condition. Next: one moving half-A pass at fixed mounting, then inspect decoded event and retained result.

## 30 October 2026 — v5.30 measured wheel response and balanced restart

v5.29 physical log11-58-31-231Z records10 recoveries,10 trim reversals, zero barcode edges and final BARCODE_ENCODER_STALL on RIGHT (1001 ms without progress, last PWM0.194/0.215). User confirms wheels were free and the car was not handled. The final stall remains a real unresolved physical observation, not assumed to be lifting. During requested left trims the encoders sometimes show greater left than right forward travel, i.e. yaw opposite to the request. After each recovery the left often moves while right barely moves. The450 cps correction trial regressed physically; software tests previously did not model unequal static friction. Stationary black confirmation does not establish vehicle heading.

v5.30 restores600 cps outside-wheel correction target;60–220 cps differential and earlier progress-aware optical trim remain. Centre-profile wheel speed feedback updates at20 ms minimum intervals instead of100 ms (actual loop can be slower). New centre-only start/restart phase verifies at least8 forward counts on each wheel. A wheel ahead by12 counts is unpowered while its partner catches up. Per-wheel effort starts at provisional0.12/0.14 minimums or bounded learned seeds, ramps at0.12 duty/s while that wheel has not reached8 counts, capped0.26. Encoder stall, sampling, timing, travel, button/sonar stop and watchdog protections remain. These seeds are diagnostic tuning, not a newly measured motor model.

After recovery, new_segment preserves bounded learned effort (feedforward plus prior integral, seeds capped0.22) while resetting velocity/count history. It still performs the balanced launch. The PI handoff is primed from startup duty. Optical trim is deferred during this small startup movement. A correction may reverse on worsening/stalled optical progress only after at least20 differential counts in the requested yaw direction. A requested turn that has not physically happened after1 s stops with CENTER_STEERING_NO_RESPONSE; the absolute1.5 s/240-count trim limits remain. This prevents treating failed motor execution as evidence that the optical direction was wrong. No geometric heading estimate or guarantee of post-recovery alignment has been added; physical verification is still required.

Validation: new unequal-friction synthetic wheel-plant tests, opposite-side friction, warm restart, bounded leading-wheel skew, blocked-wheel stops and late-loop checks. Simulated launch peak skew13.2/0.1/27.3 counts and final4 s speeds approximately595/595,599/598,597/587 cps; these are software results only. Recorded opposite-yaw regression confirms no false optical reversal. Sanitized barcode/motion suite,32 centre main and18 legacy main scenarios pass. Strict Pico W build and UF2 structure/family/version checks pass. No car operation performed. Firmware build/motion-wireless/robocar_motion_integration_v5_30_wireless.uf2; SHA256 de95e9323741d3d3b1d257cc911a335dc496f79c022690eec8b14c87513f8d26.

Next physical run should repeat the same straight-edge starting case with fixed sensor placement, default straight course actions. Inspect CENTER_BALANCED_START, actual wheel counts during correction, reacquisition behaviour and final stop. Do not increase PWM or dismiss the encoder/power path if the freely moving right wheel still reports a stall. Barcode acceptance is not the priority of this run.

## 30 October 2026 — v5.29 progress-aware forward steering

Latest runs in the cumulative v5.28 downloads (11-46-02-556Z,11-47-04-155Z,11-48-16-736Z) accepted zero letters and discarded1/1/2 active barcode fragments during recovery. Entire-run recovery counts10/11/15; all final stops CENTER_LINE_NOT_REACQUIRED. All three retained full five-second driving traces with intact timing. Passing the physical barcode is not proof of a complete decoding pass.

Old trim reversed after500 ms or120 differential counts even with improving centre reflectance. Run1 raw GP28 approximately1243→1351→1538 coincides with reversal before loss. v5.29 enters trim at85% of frozen reference, clears at92% (previously80/90). A20-unit normalized improvement renews only a progress timer. Deterioration of more than40 from the peak after80 ms/20 differential counts or500 ms without progress permits one reversal.120 stage counts reverses only a non-improving trial. Absolute1.5 s and240 accumulated differential-count limits span both trial directions and cannot be renewed by progress. Exhaustion enters the existing bounded search.

During trim the outside target drops from600 to450 cps; the tapered difference is60–220 cps, retaining positive inside targets and wheel-speed feedback. Straight travel remains600 cps. Corrected straight-speed statistics: the legacy1080 cps cutoff wrongly classified all600 cps cruising as steering; centre mode now uses540. Motor PI gains, manoeuvres, barcode thresholds/decoder, search protections and automatic driving traces remain unchanged. A centre-only reading still lacks a left/right sign; GP27 or last successful direction supplies the initial trial.

Validation: sanitized barcode/motion regressions; recorded-value steering regression; improving/stalled/deteriorating trials; absolute budgets despite progress; correction slowdown/return and straight speed statistics.32 centred and18 legacy production-main scenarios pass. Strict Pico W build and UF2 checks pass. No physical test performed. Firmware: build/motion-wireless/robocar_motion_integration_v5_29_wireless.uf2, SHA256 b8323e0093fc761b1a69f2069b4e076c84beb3bf3df1330aafbf549b728e9733.

Next: one same-track half-A driving pass with fixed mounting, default straight actions; stop before table edge, download promptly. Compare actual interruptions, reversals, wheel-speed summaries and decoded sequence. Physical steering improvement and half-A acceptance remain unverified.

## 30 October 2026 — v5.28 automatic driving optical diagnostic

Physical v5.27 evidence: `AAI10-motion-2026-10-30T11-35-45-086Z.txt` contains one course run, zero accepted barcodes, 24 edges and 23 completed runs (29 required for a framed single character), one incomplete-frame timeout, no acquisition integrity fault, max sampling gap1033 us and no fragments discarded for recovery. One early black interval lasts572 ms; this suggests missed/merged transitions but the sparse live readings cannot establish which threshold or optical effect caused them. The final stop is `CENTER_LINE_NOT_REACQUIRED`, after eight successful reacquisitions; it is not an obstacle stop. Whether the final failure occurred at the end of the physical tape is not known.

v5.28 is a diagnostic build, not a verified half-A decoding fix. Reuses the existing2 KiB raw min/max buffer to capture five seconds from the first detected barcode edge in each driving scan. Prepared in foreground with IRQ exclusion; constant-time trigger and bounded extrema collection in the existing ADC IRQ. Only one capture per run; an early spurious edge can trigger it, so use one isolated barcode. Records min/max over10 ms windows, not every raw sample or exact within-window ordering. No pretrigger history is retained. Capture freezes when motion ends; early stops are marked aborted/incomplete. Data is emitted after stopped settling, with `source=DRIVE` and trigger offset relative to run start, into the same Download log. No extra buttons or hand swipe. Download promptly, with the page open: full trace retention still depends on the rolling log/browser buffer.

Threshold226/220, ten-sample filter, decoder checks, motion tuning and all safety limits remain as v5.27. Stationary capture remains available. A new motion run clears the old retained optical summary. Browser recognises a saved driving trace without telling the user to slide paper.

Validation: sanitized acquisition/decoder/motion and optical arm/trigger/freeze tests pass;32 centre production-main scenarios pass, including full/early-ended driving captures, stationary cancellation and assertions that optical logging occurs outside IRQ with zero motor PWM. Wireless transport/retention and mocked browser tests pass, including driving trace in the common download. Strict Pico W build and UF2 block/family checks pass. No hardware flash or physical test performed. Artifact: `build/motion-wireless/robocar_motion_integration_v5_28_wireless.uf2`, SHA256 `7a605d8c76a4f9c0d966da3ac65db569f7c81530e98c3c4128fcdef97c6deb1e`.

## 30 October 2026 — v5.26 simplified calibration UI

The user's v5.25 download AAI10-motion-2026-10-30T10-58-58-268Z.txt contains the full optical capture:5000 samples, raw172..2782, maximum gap1017 us, no empty10 ms windows, no abort. All500 extrema windows are present. The first roughly2.4 s stay near250..278, followed by two prominent high peaks, then mostly low readings. Capture integrity is confirmed, but this unlabeled sweep does not establish narrow-bar separation or a complete decoded barcode. No threshold or driving change is justified from this trace alone.

v5.26 replaces the long instruction page with one test dropdown and three visible buttons: mode-specific Record sensor / Prepare run, Stop, Download log. Defaults to motors-off barcode sensor check. Motion options, full Y/Z actions and junction advance remain accessible; Y/Z settings are collapsed and only relevant during track mode. One shared download includes optical and motion records; redundant recovery button removed, retained results load on reconnect/test completion and at download. Live technical log is collapsed, sensor summary is readable text and the graph scales to phone width. Physical GP20 still starts driving; stationary optical capture starts from its explicit Record button, never starts motors, and GP20 cancels it. Motor, line-following, decoder and sensor thresholds unchanged from v5.25.

Validation: browser mocks exercise all six test choices, all retained action dropdown options, numeric validation, default straight mission, optical trace fragments, one combined download, automatic result retrieval, busy/offline lockout, Stop, reset clearing, responsive graph and three-button layout. Wireless transport/retention tests and strict Pico W build passed. No physical flash or motor run performed. Artifact: build/motion-wireless/robocar_motion_integration_v5_26_wireless.uf2.

## 30 October 2026 — v5.25 stationary optical trace; half-barcode contrast unresolved

User reports v5.24 completed the home route. Latest stationary screenshots show GP26 wide white185–190, narrow white200–218, wide black1067–1081, narrow black214–219. Narrow classes overlap. No defensible separating threshold follows from these samples; black entry251 and white release220 remain unchanged. GP28 following, frozen reference, search bounds, speed and mission behaviour are preserved. Do not label this as a half-barcode decoding fix.

Added web button “Inspect barcode signal · 5 seconds” (serial v): stationary capture only, no GP20 start required. Existing1 ms ADC IRQ records min/max in500 successive10 ms windows; brief peaks are preserved while exact within-window pulse timing is not. Fixed trace buffer is approximately2 KiB. No heap allocation, alternate ADC owner or ISR logging. Foreground prints bounded numeric trace lines and summary after capture, showing sample count, missing windows, maximum sample gap and abort flag. Sampling_ok means uninterrupted capture, not readable barcodes. Browser graphs the extrema and includes them in Download log; latest optical summary is separately retained through browser disconnect, but the full trace relies on the rolling log/browser download. Download immediately before another capture or power reset.

Captures are rejected during movement, countdown or settling. Physical GP20 and browser stop cancel a capture without starting a move. Selection and terrain calibration are blocked while capturing. Capture ends after5 s even if the ADC timer stops. Inspect the half-size strip in a slow stationary swipe to check for peaks missed by the250 ms screen or imperfect alignment, without guessing missing stripes. No automatic threshold adjustment or relaxed decoding introduced.

Validation: sanitized optical envelope tests for brief peaks, missing windows, clock wrap and cancellation; barcode/motion regression suite;32 centre production-main scenarios including optical timed completion and GP20/browser cancellation with zero motor PWM; wireless command/retention/HTTP tests; mocked browser trace parsing across fragmented log responses, graph, download, busy/disconnect/reset handling and mobile width. Strict Pico W build and UF2 structure/version checks passed. Physical optical-sweep and half-size decode validation remain pending. Artifact: build/motion-wireless/robocar_motion_integration_v5_25_wireless.uf2.

## 30 October 2026 — v5.24 stable tape reference and stopped confirmation

Latest v5.23 evidence (`AAI10-motion-2026-10-29T14-31-32-896Z.txt`) shows three `CENTER_LOCAL_RECOVERY_EXHAUSTED` stops. In the first, the running maximum reference rose to797; normal-looking GP28 raw1919 remained below its approximately2026 reacquire threshold. Another run entered confirmation at raw2002 then exhausted recovery; soft-boundary/coast interaction is a supported code failure mode, not proven from the sparse live samples alone. Barcode results A / empty / empty, without rejection, timeout or integrity faults.

v5.24 removes running-maximum adaptation. During the stopped countdown, collect the last31 normalised GP28 observations at least10 ms apart. Median becomes the fixed per-run reference; require all31, last sample no older than100 ms, median>=400 and interquartile range<=20 percent of median. Weak/unstable placement rejects start as `CENTER_BASELINE_NOT_READY` and prints median/spread/count. Reference is set once at mission start, with no running optical sample allowed to raise it. Retained course result includes fixed reference.

A strong candidate must first appear inside the existing sweep travel budget. Motor command then stays zero throughout confirmation. Permit up to80 extra counts of coast per wheel from candidate detection, require50 ms settled (movement beyond2 counts resets this timer) and30 ms continuous black, with300 ms confirmation timeout and existing3 s total search limit. During this stage use70 percent reference as black hold threshold instead of85 percent entry; both have normalised300 floor. Excess coast, lost/unsettled confirmation or late control stop with distinct reasons. No powered extension of the sweep and no success on black first seen beyond the sweep cap. Retained result reports per-wheel confirmation coast. This is a bounded software allowance, not a measured physical stopping-distance guarantee.

Validation: new sanitized regressions for median/outliers, stale/weak/unstable baseline, baseline replacement, frozen reference under log-like797/637 readings, successful zero-power confirmation after crossing the soft60-count boundary, lost black, excess coast and late control.29 centred production-main scenarios, including startup spike rejection by median, frozen reference during higher running readings and unstable-start rejection;18 legacy scenarios; existing barcode/motion suites, wireless retention/transport, mocked browser page/download and strict Pico W build/UF2 checks passed. No physical acceptance yet. Barcode capture/thresholds and mission action semantics unchanged. Next physical check is the same full A then half A route, Y/Z straight, GP28 centred and stationary for countdown; download before restarting and stop before lifting. Confirm stable tracking first, and require actual `sequence=AA` before claiming both symbols were read.

## 29 October 2026 — v5.23 early optical steering and local recovery

Supersedes v5.22's immediate barcode-area stop. Latest log `AAI10-motion-2026-10-29T14-13-45-254Z.txt` has three preserved stops with `CENTER_LOST_NEAR_BARCODE`; first retained run read one A, later stops had no complete symbol. Latest stopped after18 edges, with zero rejection/timeout/integrity fault. The issue was controller policy interrupting a partial scan after GP28 loss, not a decoder-requested stop. v5.22 only maintained straight encoder balance while GP28 saw black; it lacked early optical steering.

v5.23 keeps GP28 centre / GP27 right / GP26 barcode mounting. It learns a strong GP28 reference (normalised500 minimum,900 maximum; update during following with GP27 white). Forward correction starts below80 percent, clears above90 percent. It slows the inside wheel by100–300 counts/s while the outside target stays600, keeping both targets positive. GP27 black is a rightward hint; otherwise use the last successful direction or probe left initially. A worsening reading after80 ms and20 differential counts reverses the trial. Each leg is limited by120 differential counts or500 ms, with one reversal; exhaustion enters bounded recovery. Centre weak plus right black no longer counts as a crossing; strong centre plus right black remains a provisional crossing candidate. Barcode capture remains active during forward trim.

Full line loss still brakes. Automatic searches normally allow120 wheel counts first sweep /240 after reversal. Recent GP26 activity now tightens that to60/120 instead of immediately stopping; further activity never resets the search origin/budget. Failure reports `CENTER_LOCAL_RECOVERY_EXHAUSTED`. Strong confirmation requires85 percent of the reference (normalised400 minimum) for30 ms. Encoder travel/direction limits are checked before accepting black. Commanded-turn reacquisition remains400/800. Pivot/search fragments remain discarded; a narrow recovery can therefore save tracking without preserving an interrupted barcode. This does not identify ink semantically or guarantee clearance on a track of unknown spacing.

Diagnostics add `CENTER_TRIM_LEFT/RIGHT`, reference, requested steering difference, trim attempts/reversals in live and retained results; line-presence logging uses the same hysteresis state as the controller. Dedicated barcode thresholds, calibrated fixed-distance/angle routines, GP20/web stop, obstacle and motor/sampling fault protections retained. No specific home-track corner positions or expected A letters are encoded.

Validation: sanitized controller/barcode suites;27 centred production-main scenarios including left/right trim during a complete barcode, bounded failed local recovery, successful short recovery followed by a subsequent valid barcode;18 legacy main scenarios; wireless retention/transport and mocked browser checks; strict Pico W build/UF2 validation. Physical outcome pending. Flash v5.23 and run the same full-A then half-A pass with Y/Z set straight. Confirm smoother tracking and examine sequence, trim/recovery counts and stop reason; do not assume half-A success without `sequence=AA`.

## 29 October 2026 — v5.22 centre-edge recovery and barcode-zone guard

Evidence: `AAI10-motion-2026-10-29T13-47-18-338Z.txt` contains three preserved GO sections plus log gaps, so the four user-reported tests cannot all be mapped confidently. First complete and latest retained results each say `reads=1 sequence=A timeouts=1 edges=42`, with maximum sample gaps 1029/1026 us and no integrity fault. First A at 10745 ms was followed by only 12 further edges and a timeout: no demonstrated full-A/half-A pair. Another preserved run has one A, 22 recoveries and an obstacle stop. User observed edge stutter and a recovery onto barcode ink. Home tape-to-barcode gap approximately 25 mm; school gap unknown.

Changes: GP28 follow-presence hysteresis uses normalised 250 enter / 200 leave (approximately raw861/724 with existing calibration). Search confirmation requires normalised400 (raw1270) continuously for30 ms, instead of accepting the weak edge as found. This moves the acceptance point further into black; it does not geometrically measure the tape midpoint. Automatic loss sweeps reduced from400/800 to120/240 counts, still bounded by timing, wrong-direction and per-wheel progress checks. GP26 black or confirmed transitions timestamp recent barcode activity; automatic line loss within2 s stops as `CENTER_LOST_NEAR_BARCODE`, including activity detected during a search. This prevents that automatic search, not all possible barcode/tape confusion. Conservative guard may stop at a genuine bend beside a barcode, and a barcode not seen by GP26 cannot activate it. No assumed school spacing or course-specific turn position. Explicit commanded-turn reacquisition retains400/800 bounds and is not covered by the automatic-loss guard.

Barcode thresholds and1 ms sampling unchanged; incomplete frames are never guessed. Added failure widths, and retained final-result fields for last ended capture widths, active run count and discarded fragments during braking/search. These remain available when live log history is lost. `last_ended_*` refers to the last completed/timed-out/rejected capture, not necessarily a particular physical sheet.

Validation: sanitized controller/barcode suites including threshold hysteresis, strong/flickering reacquisition, shorter search bounds, guard before/during recovery, expiry, and synthetic full A followed by half A;24 centred and18 legacy production-main scenarios, wireless retained-result/transport checks, mocked browser downloads/lockout/mobile layout; strict Pico W build and UF2 structure/version validation. No new hardware pass yet. Full-size A physically demonstrated; half-size A remains unresolved. Next: flash v5.22, same mounting; one full-A then half-A pass with straight exit, both Y/Z actions straight. Stop with GP20 before lifting; download before another run. Observe whether edge chatter improves or the explicit guard stops the run. Success requires `sequence=AA`, not merely visible transitions.

## 29 October 2026 — v5.21 GP28 centre-line mounting trial

User explicitly chose GP28 directly over tape, GP27 to its right, GP26 dedicated to barcodes. Build enables ROBOCAR_CENTER_LINE. Legacy straddling code remains available only in a separate build; its moving b/c/t/n diagnostics are rejected in this build and removed from the page. Fixed-distance/angle diagnostics remain. Startup mission requires fresh GP28 black. UI and log roles identify centre rather than line-left.

Centred tracking uses encoder-controlled 600 counts/s. Centre loss brakes immediately and pauses barcode interpretation. GP27 black selects right-first search; otherwise it probes left, then reverses. Provisional search bounds: 400 counts first sweep, 800 on return, 1.2 s per sweep, 3 s total, 50 ms braking before each direction, 18 percent opposed PWM, per-wheel 1 s progress check and wrong-direction checks. Resume requires GP28 black for 30 ms, with reset odometry segment; a timeout never resumes forward driving. Search also replaces post-manoeuvre reacquisition in the centre build. Latest deferred instruction activates on recovery; STOP overrides. Recovery count/layout are retained in the downloaded result.

Both GP28/GP27 black still denotes a crossing candidate, not proof: wide bends and lateral spacing can cause false positives. No claim of physical junction-count acceptance. Sensor thresholds/height, search angle/power, speed and barcode optical contrast remain physical trial items. After relocation keep original forward lead/height; test one bend with a straight exit before barcode runs. No circuit-specific turn positions or barcode meanings added. No code copied from the external reference project.

Validation: sanitized controller and recorded barcode suites; 21 centre-layout production-main scenarios (including successful/failed recovery and start rejection), 18 legacy production-main scenarios; wireless retained-result/transport tests; mocked browser selection, downloads, lockout and mobile layout. Strict Pico W build and UF2 checks. Physical centre-layout validation pending.

## 29 October 2026 — v5.20 steering power retention and stall evidence

User reports a tight left correction followed by a stop. Fixed repeated clearing of positive speed-loop integral during nonzero inside-wheel commands; this could prevent recovery power from accumulating during gentle corrections. Intentional zero-target pivots still clear power and integral. No unmeasured minimum-duty floor introduced. Speed targets, barcode decoding, crossing policy and one-second encoder-stall guard retained.

Added wheel-specific stall mask, no-progress ages, last commanded targets/PWM and last measured speeds to the retained MOTION STOP line, so the download retains evidence even with live Wi-Fi log gaps. This identifies the monitored wheel; it does not by itself distinguish a stuck wheel from a missing encoder signal. Diagnostics use the controller segment time, including after manoeuvres.

A bilateral repeated-edge regression fails on v5.19 and passes with the fix; checks also verify the one-second stall still fires, an intentionally stopped inside wheel is exempt, and the driven outside wheel remains monitored. Sanitized controller/barcode suites, 18 production mission-loop scenarios and wireless result/transport tests passed. Strict build and RP2040 UF2 structure/version checks passed. Physical benefit remains pending; exact cause of the previous stop is not proven. Flash v5.20, select Y/Z mission with both actions straight, run through the same bend/barcode, stop with GP20 before lifting and download the log.

## 29 October 2026 — v5.19 no overall course timer

User requested removal of the course timer. Removed only the overall course wall-clock cutoff; retained 160000-count travel bound, GP20/web stop, obstacle and all motor/sampling/manoeuvre fault checks. Latest school log `AAI10-motion-2026-10-29T07-07-35-729Z.txt` actually stopped on OBSTACLE_STOP at 29.9 cm, not time. Five crossing candidates, zero commanded turns, zero valid barcode reads, one rejected capture, nine timeouts, max sample gap 1051 us with no integrity fault. Raw transitions do not establish complete barcode capture; live-log gaps prevent full reconstruction. Speed/thresholds unchanged from v5.18. Sanitized unit suites and all 18 production main scenarios pass; added past-five-minute controller check while retaining travel/stall tests. Strict build and RP2040 UF2 validated; hardware retry pending.

## 29 October 2026 — v5.18 straight-speed trial

School log `AAI10-motion-2026-10-29T06-56-44-021Z.txt` ended with COURSE_TIME_LIMIT after 120 seconds, four crossing candidates, zero commanded turns and zero invalid encoder transitions. User reports successful following, wrong direction for barcode placement. Increased continuous/course straight target to 1200 counts/s only after 250 ms without an edge; bends and course crossing/centering remain at 900 counts/s, with prior tight-corner inside-wheel cutoff. Course test cap is now 300 seconds, retaining 160000 counts and existing stop protections. Unit/decoder/guard/motion suites and 18 production main-loop scenarios pass; new tests verify delayed cruise and immediate edge slowdown. Strict build and RP2040 UF2 validated. Straight target +33 percent is not a measured lap-time gain; no guarantee of corner or barcode performance at this speed. Flash/retest pending.

## 29 October 2026 — v5.17 removes barcode-white start gate from line-following runs

School log `AAI10-motion-2026-10-29T06-35-24-740Z.txt` showed successful Y/Z selection and three GP20 countdowns rejected by the barcode-white gate (later background readings around 300–360 versus the 220 start threshold). Y/Z mission and Continuous scan now start with fresh IR sampling regardless of barcode colour. Decoder waits independently for 250 ms white, avoiding a partial starting symbol; single-pass barcode diagnostic retains its white-start requirement. Start rejection now sets an explicit stopped reason instead of leaving MOTION_COUNTDOWN. v5.16 steering retained. Strict build and all 18 sanitized production main-loop scenarios passed, including permanent black, observed-like 330 background, and black-start followed by white and a valid Y. UF2 validated; hardware retest pending.

## 29 October 2026 — v5.16 physical corner tuning candidate

Two v5.15 Continuous scan runs understeered on the real track. Built v5.16 with a shared 900 counts/s continuous/course profile, prompt inside-wheel slowdown, an 80 ms sustained-edge drive cutoff and a bounded 100 ms correction hold. Crossings clear correction memory; calibrated manoeuvre targets and Y/Z route semantics remain unchanged. Software checks passed; actual corner improvement remains unverified until the next run. See [corner evidence and retest](testing/course-v5_16-corner-steering.md).

# Development and test status

**28 October: v5.15 Y/Z-triggered mission built; physical validation pending.**
Replaces the run-relative selector with independent Y and Z dropdowns: immediate
turns, future-junction turns, skip-then-turn, straight/cancel and stop. New reads
replace waiting instructions; committed manoeuvres defer the newest valid read;
Stop overrides. Barcode capture pauses during pivot/braking/search. User-defined
test meanings only. [Policy, controls and tests](testing/course-v5_15-barcode-actions.md).

**28 October: v5.14 school-course test built; physical validation pending.**
Separate mode adds stronger curve steering, debounced crossing encounters and an
optional one-shot left/right turn at encounter 1–9. User measured line sensors
120 mm ahead of axle; nominal centering remains adjustable and unvalidated.
Default straight-through mode records A–Z; no invented barcode mission meanings.
Controller/main-loop simulations, regressions, wireless/UI tests and strict build
checked. [Scope and school test order](testing/course-v5_14-school-test.md).

**28 October: v5.13 continuous scan built; physical validation pending.**
New separate mode records successive letters and continues after reads/rejections,
rearming after a 250 ms white gap. Brief both-black readings use bounded slow
straight bridging; persistent ambiguity and existing fault/obstacle guards stop.
20-second/24,000-count test bounds remain. Software/build/mock UI checks passed.
No mission turns, bypass or cliff detection. [Procedure and scope](testing/barcode-v5_13-continuous.md).

**28 October: v5.12 physical A and Z decode; repeated central-line stops.**
A1 and Z2 decoded. The Z export contains five attempts; Z1/Z3/Z4/Z5 stop on
BOTH_LINE_BLACK before a complete frame, with no decoder/capture fault. User
emphasizes continuous assessment operation; the current single-frame diagnostic
is not mission firmware. [Evidence and scope](testing/barcode-v5_12-physical-line-stops.md).

**28 October: v5.12 decoder build ready; physical check pending.** Per-symbol
black/white timing-bias correction reads nine recorded complete A/Z frames,
including the v5.11 Z1/A2 rejections. Strict separation, coherence, exact guards
and failed-scan stops remain. Steering/thresholds unchanged; negative-pattern,
packed-storage, capture/motion/wireless tests and build pass.
[Method and checks](testing/barcode-v5_12-width-bias.md).

**28 October: v5.11 failure stops physically verified; decoding still mixed.**
Z outcomes reject/read/read; A outcomes read/reject/timeout/read. User identifies
A3 as deliberate misalignment. Full-frame Z1/A2 failures reproduce the decoder
width-ratio guard; all sampling/encoder fault counts are zero.
[Evidence and next investigation](testing/barcode-v5_11-Z3-A4-result.md).

**28 October: v5.11 built, physical validation pending.** Moving barcode tests
stop on the first rejected frame/element timeout; steering uses ramped900/1200cps
targets with continuous feedback instead of55% outer-wheel PWM. Raw-gap counters
and target logs added. Recorded-run replay, host regressions, strict build and
mocked UI checks pass. [Details](testing/barcode-v5_11-failure-stop-steering.md).

**28 October: v5.10 barcode repeatability remains open (3/5).** A1/Z2/Z3 decoded;
A2 captured a complete frame but failed a width-ratio check; Z1 missed transitions
and timed out. Both failures continued until BOTH_LINE_BLACK, not fall detection.
Sampling/encoder faults were zero. [Analysis](testing/barcode-v5_10-A2-Z3.md).

**28 October: v5.10 physical speed/obstacle check completed.** Mean clear-interval
wheel rates1172/1155cps versus1200; obstacle stop at sonar27.2cm; user reports33cm
travel. No encoder/capture faults. Sonar long-range outliers remain unresolved.
Next: A/Z decoding at the revised speed using the same firmware.
[Evidence](testing/barcode-v5_10-speed-obstacle-result.md).

**28 October: v5.10 speed-feedback build ready; physical test pending.**
Barcode-pass starting power and speed feedback revised using v5.9 logs; retained
line corrections, barcode thresholds and calibrated manoeuvres. Build, host
regressions and mocked browser tests pass. Next: combined line/speed/box run.
[Changes and procedure](testing/barcode-v5_10-speed-feedback.md).

**28 October: v5.9 obstacle stop physically demonstrated.** User confirms line
following and approximate20–25cm obstacle clearance. Log stops on OBSTACLE_STOP
at sonar26.7cm, settles near25–26cm with zero encoder/capture faults.30cm is a
two-reading trigger, not a guaranteed final clearance. Speed tracking remains
open; sonar outliers and full bypass are not validated by this run.
[Evidence](testing/barcode-v5_9-obstacle-stop.md).

**28 October: v5.9 physically decoded A and Z successfully.** One user-labelled
pass each,30 transitions,1 accepted frame,0 rejected frames and no encoder or
capture faults; automatic stop on BARCODE_READ. A log confirms restored0.25/0.55
left correction, but visual tracking/right correction still need confirmation.
Current-version repeatability tally A1/3,Z1/3. Speed target tracking remains open.
[Evidence](testing/barcode-v5_9-A-Z-success.md).

**28 October: A barcode decoded successfully in two v5.8 driving runs.** Both
captured30 edges with no sampling/encoder faults and stopped on BARCODE_READ.
User reports weak line following; logs show detected left edges with too-small
steering differential. v5.9 restores immediate0.25/0.55 line-edge response,
preserving barcode thresholds and calibrated manoeuvres. Software/build checks
pass; physical correction and Z decoding pending. Measured wheel speeds exceed
the nominal1200cps target, so speed regulation is not yet validated.
[Evidence and change](testing/barcode-v5_9-line-priority.md).

**28 October: v5.8 controlled-drive barcode firmware built.** Barcode pass now
regulates encoder speed at a nominal1200cps with251/220 thresholds and retained
edge traces. Original motion calibration and bounded stops preserved. Build,
controller/acquisition/regression/transport and mocked-browser tests pass.
Physical movingA test pending. [Build and run procedure](testing/barcode-v5_8-controlled-drive.md).

**28 October: v5.7 now captures complete barcode patterns physically.** Two
hand swipes each produced30 edges with no data faults. Both rank as reversed*A*,
but both fail the current width-separation check; no letter accepted yet.
Controlled-motion validation is next, rather than more identical hand swipes.
[Capture and decoder replay](testing/barcode-v5_7-two-complete-frames.md).

**28 October: v5.7 fixes a reproduced stationary timing-check race.** v5.6
physical capture aborted at6.492s before the user's barcode sweeps; this is not
a threshold-validation result. New build takes a coherent timestamp snapshot
and logs fault details, preserving251/220 thresholds. Eighteen host scenarios
and build/transport checks pass; physical test pending.
[Evidence and fix](testing/barcode-v5_7-freshness-fix.md).

**28 October: v5.6 barcode threshold built, physical test pending.** User-labelled
readings show narrow black254–260 versus white182–202. Changed GP26 barcode
black-on to251 (>250), white-release to220, retaining two-sample confirmation.
Sixteen simulated stationary scenarios and regression/build checks pass. v5.5
physical scans captured six black regions per slide and no complete barcode.
[Changes, evidence and next run](testing/barcode-v5_6-threshold.md).

**28 October: v5.5 stationary barcode diagnostic prepared.** Dedicated firmware
holds both motors off, permits slow hand scans, and retains up to128 transitions
across decoder timeouts. Physical validation pending. Previous v5.4 hand slides
while idle did not decode; latest recovered result was stale. Moving capture and
recurring encoder errors remain unresolved. [Procedure and tests](testing/barcode-v5_5-stationary-scan.md).

**28 October: v5.4 barcode diagnostic built; physical test pending.** Added 1 ms
alarm-owned ADC acquisition, a bounded timestamped edge queue, and a separate
Barcode pass (read / 8 s / 8000-count stop). New summaries expose sampling gaps,
queue overflow and captured widths. All four motion targets retained. First run:
large A, then large Z and half-sized samples. [Procedure and evidence boundaries](testing/barcode-v5_4-moving-pass.md).

**v5.3 physical line check:** seven logged runs, six travel-limit stops and one
intentional both-black stop, all with zero invalid encoder transitions. User confirms
correction toward the line from angled starts; smoothness remains open. Apparent
pitch spans -16.5 to +9.3 degrees, with no completed new humps; flat-surface/handling
confirmation pending. No tuning changed. [Evidence](testing/line-v5_3-physical-results.md).

**v5.3 role mapping completed:** user confirms GP26 offset barcode, GP28 line-left,
GP27 line-right. Firmware isolates these roles and adds a GP20-started, two-second
line check with a wheel-count cap and stop conditions. Barcode detections are record-only;
all four motor targets are unchanged. Software checks passed; physical line steering
is pending. [Details and next run](testing/ir-v5_3-role-mapping.md).

**Latest stationary IR check:** all three electrical channels show clear light/dark
contrast in the unordered sweep. Existing thresholds separate the observed low/high
clusters. Physical role mapping still needs two identified placements (offset barcode,
central-left); do not infer roles from sweep order. No firmware change required for
that check. See [contrast evidence](testing/ir-v5_2-unordered-sweep.md).

**Latest v5.2 physical result (26 October):** user confirms right90 corrected;
completed right90 and straight runs show zero invalid encoder transitions. Keep
motion targets unchanged. Straight pass shows strong contrast on IR_C only. User
clarifies two central line sensors plus one offset barcode sensor; current firmware
still uses a three-channel barcode vote, so channel-to-role identification and
logic adaptation are required before line-following integration. See
[run evidence and next check](testing/motion-v5_2-right90-ir-pass.md).

Last updated: 26 October 2026 (historical component results retained below).

**v5.2 update:** new physical v5.1 log contains completed L90, two R90 and two R180
runs, all with zero invalid encoder transitions. User reports straight30 and L90/R180
good, R90 about 85 degrees. v5.2 adds a provisional +58-count R90-only trim, combined
IR/sonar/pitch/run timing summaries, removes UI feedback fields and adds an independent
motor service alarm after an observed idle radio stall. Other targets unchanged.
New physical verification pending. See [v5.2 evidence](testing/motion-wireless-v5_2-combined-check.md).


**26 October physical v5.1 update:** wireless page selection and log export now
worked on the car. The supplied session contains three movements aborted by sonar
OBSTACLE stops (18.3, 25.8, 25.0 cm) and two right-180 starts blocked immediately
(17.7/18.1 cm). None completed its target. User estimates of 15 cm/45 degree travel
are observations of aborted moves, not evidence for changing distance or turn
calibration. Investigate sonar detection/mounting and clear-space operation first.
See [the preserved log analysis](testing/motion-wireless-v5_1-obstacle-stops.md).


**26 October follow-up: v5.1 DHCP correction built.** v5 associated with the
Mac but failed address assignment (self-assigned 169.254 address). An actual-lwIP
packet regression reproduced the initial 0.0.0.0 source being dropped before
DHCP. The corrected server-port accept configuration passes the complete DHCP
exchange in that test. On-car v5.1 verification remains pending; motor tuning
is unchanged. See [wireless calibration](wireless-calibration.md).

**26 October: v5 wireless calibration built**, continuing the v4 30 cm/end-pivot
check with unchanged motion tuning. The car hosts its own protected Wi-Fi network
and browser page; USB/VS Code output remains available. Host logic/transport tests,
browser tests with a simulated car, and firmware compilation passed. Physical
Wi-Fi/battery behaviour and the v4 balancing result are still pending. See
[wireless calibration](wireless-calibration.md). This diagnostic build is not the
final MQTT/RTOS integration or the complete minimum-bit implementation.

Implementation update, 21 October 2026: the existing Batch 5 controller has now
been ported into the main firmware, with USB diagnostic selection and packed
barcode capture. Host checks and the Pico W build passed; broader physical
integration testing remains pending. The verified results below remain the historical
hardware baseline. A subsequent first physical straight run completed with
settled counts 4272/4144 and zero invalid transitions. The operator estimated
about 35 cm; no measured distance was provided, so distance accuracy remains
open. See [run evidence](testing/motion-integration-2026-10-21.md) and
[motion integration](motion-integration.md). Do not
restart completed component calibration solely because this build is new.

A subsequent ruler-measured run travelled 340 mm for the 300 mm command,
with settled counts 4289/4276 and zero invalid transitions. v2 applies a
provisional straight-only ground scale (12.595588 counts/mm; target 3779)
and checks endpoints each loop. The subsequent supplied log contains two
completed runs, with means 3871.5/3869.5 counts and zero invalid transitions;
the operator reported 320 mm travel. v3 now uses a provisional 3537-count
30 cm endpoint, with physical verification pending. See
[measured run and correction](testing/motion-integration-distance-v2.md).
See also [v2 results and v3 endpoint](testing/motion-integration-distance-v3.md).

The operator then reported 30 cm with minimal error on v3, with settled counts
3631/3627 and zero invalid transitions. A small end-left pivot was observed;
the left wheel had led by up to 94 counts before the final catch-up. v4 keeps
the 3537-count endpoint and adds bounded straight-wheel balancing and cutoff
timestamps. Its physical result remains pending. See
[v3 observation and v4 change](testing/motion-integration-stop-yaw-v4.md).

## Verified on the assembled robot

### Motor bring-up

- Both motors have been driven in the expected forward direction.
- The right motor polarity was corrected and reverified during the raised-wheel
  encoder motor test.
- GP20 start/stop behaviour was demonstrated in the integration prototype.
- Both encoder A/B channels have been hand-rotation tested with no invalid
  transitions. Forward count signs are left negative and right positive.
- A repeated ten-revolution hand test measured approximately 2539 counts per
  revolution on the left and 2547 on the right; 2543 is the initial shared
  calibration value.
- At equal 35% PWM with both wheels raised, the left wheel measured about 3229
  counts/s and the right about 3393 counts/s. The right wheel was 4.9% faster;
  both directions were correct and both invalid-transition counts remained zero.
- The standalone PI controller targeted 3000 counts/s and measured 3000 counts/s
  left and 3017 counts/s right over eight seconds. It reduced the mismatch to
  0.5% using approximately 32.5% left and 30.8% right PWM, with zero invalid
  transitions. Integration into the main motion module remains pending.
- The Batch 5 floor test completed 30 cm straight, right 90 degrees, left 90
  degrees, and right 180 degrees in one run. The operator reported that the
  motions looked good. Final straight counts were 4255 left and 4217 right;
  peak wheel-count skew during travel was 56 counts. All four stages reported
  zero invalid encoder transitions. This is a visual floor check, not a
  measured-angle or full-course acceptance test.

### Ultrasonic sensor and servo

- HC-SR04+ distance readings respond to suitable front-facing targets.
- Coarse and fine servo scans complete successfully.
- The scan range is restricted to 50-125 degrees to respect the mechanical installation.
- Small or edge-on objects may reflect sound away from the receiver and return `no echo`.
- The calculated scan footprint is not yet a calibrated physical object width.

### GY-511 accelerometer

- I2C communication and live acceleration readings are working.
- Automatic two-second level calibration is working.
- Calibration is rejected when the car moves excessively during sampling.
- Climbing, descending and level orientations are distinguishable.
- A state machine detects normal and back-to-back hump sequences.
- Incomplete or timed-out sequences are cancelled.
- Hump count and highest climb angle are reported.

Bench demonstration result:

| Hump | Peak climb | Peak descent | Duration |
| --- | ---: | ---: | ---: |
| 1 | +24.3 degrees | -5.2 degrees | 3110 ms |
| 2 | +36.6 degrees | -6.4 degrees | 8292 ms |

The second hump was correctly reported as the highest climb angle. Actual hump-height estimation and real-track tuning remain pending.

### IR sensors and line following

- All three analogue IR sensors have been measured on the light styrofoam and dark line.
- Physical left/centre/right order is Grove 5/7/6, using GP26/28/27 respectively.
- Three-sensor following, GP20 start/stop, short-gap crossing, and bounded lost-line recovery are implemented.
- Serial diagnostics report the run state, line state, sensor readings, drive commands, range, and terrain status.
- Straight-line bench testing has begun; full-course line and junction performance is not yet verified.
- A motor-disabled line/junction diagnostic now records stable three-sensor
  patterns and flags broad `111` regions as junction candidates for physical
  course testing.

### Barcode and navigation commands

- A reusable command module maps the brief's example symbols A-D to left,
  right, straight and U-turn requests.
- A compact Code 39 decoder recognises one A-Z letter between `*` start/stop
  markers from 29 alternating bar/space widths. It accepts either travel
  direction and uses relative widths rather than a fixed speed or scale.
- A fixed-size reader captures full-width black/white transitions using a
  two-of-three sensor majority and suppresses repeated commands for three
  seconds. Sustained junction-like readings time out without producing a
  barcode command.
- Host tests pass for A-D navigation commands, the supplied A and Z samples,
  live capture, scaled and reversed input, duplicate suppression,
  invalid-pattern rejection and junction timeout.
- Left, right and U-turn commands carry requested angles for the future motion
  subsystem. Until that subsystem consumes them, the integrated firmware
  stops safely and reports the requested turn; straight commands continue.
- Sensor thresholds, the 750 ms edge timeout and the 3 s duplicate window
  still require physical validation on the printed course.
- Accurate command execution remains dependent on integrating the calibrated
  encoder-based distance and turn control into the main firmware.

### Integrated safety behaviour

- The robot starts with both motors stopped and waits for GP20.
- Two consecutive valid ultrasonic readings below 30 cm stop and disarm the motors.
- A missing ultrasonic echo is treated as unknown, not as a confirmed clear path.
- The sonar stop is not an obstacle-bypass implementation.

## Standalone firmware tests

| Program | Purpose |
| --- | --- |
| `robot_bringup` | Combined hardware connectivity and manual actuator checks |
| `servo_range_test` | Establish safe servo travel |
| `ultrasonic_scan_test` | Coarse/fine obstacle scans and profile diagnostics |
| `imu_hump_test` | Automatic level calibration and multi-hump detection |
| `ir_calibration_test` | Capture light/dark values for three IR sensors |
| `encoder_wiring_test` | Motors held off; report A/B edges and quadrature counts while each raised wheel is turned by hand. Hardware verified; no invalid transitions observed. |
| `encoder_motor_test` | With wheels raised, run left, right, and both motors at 35%, followed by a 3000 counts/s PI-controlled stage. All stages hardware verified. |
| `motion_control_test` | Defaults to one run of straight 30 cm, right 90 degrees, left 90 degrees, and right 180 degrees; individual motions remain selectable with GP21. Each completed stage is saved to flash and the full report is replayed after reconnection. Batch 5 completed all four stages; the operator reported good straight and turn behaviour, and every stage had zero invalid encoder transitions. This standalone calibration is ready for main-firmware integration. |
| `line_junction_test` | Motors held off; record stable three-sensor patterns and junction candidates while the car is moved manually over the course. |

## Next development priorities

1. Check the newly ported speed and turn control in the main robot firmware
   against the existing Batch 5 baseline; repeat calibration only if a specific
   fault or changed hardware provides a reason.
2. Test the three-sensor line follower and recovery on the actual track; tune speed and thresholds.
3. Validate barcode navigation on the physical course; implement obstacle bypass and Wi-Fi/MQTT telemetry.
