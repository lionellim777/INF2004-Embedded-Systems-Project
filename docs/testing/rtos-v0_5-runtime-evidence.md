# v0.5 runtime measurement candidate

Branch `yixuan/rtos-runtime-evidence`. Implementation and software checks are
complete. **Installed and stationary observations verified on 9 November**;
see [the real USB/MQTT measurements and limitations](rtos-v0_5-stationary-acceptance.json).
The first hardware check exposed a burst exceeding the four lwIP request handles.
Paced/retried publication fixed it; all four task records now arrive. The
first-passing v0.4.2 private backup is preserved. No teammate module, scanner algorithm,
steering calibration or motor-service threshold changed in this candidate.

## Implemented

- Stack allocation/request, remaining marker prefix, observed watermark,
  priority, observation flag and low-stack marker for all four application tasks.
- Per-task loop count and maximum entry gap/elapsed work duration, with lifetime
  maxima separated from each armed run. Stopped reconnects cannot inflate the
  driving-only maxima. Elapsed work includes preemptions/IRQs, not pure CPU use.
- Maximum observed guard spacing and motor-service age while armed. Idle does
  not enlarge these values; arming resets them.
- Stop sequence/source/reason and timestamps. API timing begins at `board_stop`
  entry, including delay before its IRQ lock; guard timing begins at its sampled
  observation; output rejection timing begins at the gate check. The endpoint
  follows both PWM-off writes. The first effective stop wins.
- Stack-monitor count/duration and compiler `.su` records. Identical binary
  observations reach existing USB/MQTT logs and JSON summaries.

These are observations, not worst-case guarantees. Do not sum preemption-
inclusive task durations to calculate CPU utilization. Button-edge detection
delay, physical braking time/distance, main/startup stack use and free heap are
not measured. The 40ms service deadline and 5ms hardware guard are unchanged.

## Stack method

Generated kernel configuration enables `USE_DBGSPT=1` for public `td_ref_tsk`.
The pinned vendor dependency and copied `mtk_bridge.c/h` are unmodified. A new
observed-start wrapper uses the same dynamic `tk_cre_tsk` request and attributes.
It does not add static stacks or move them out of the existing32KiB kernel pool.

The ARMv6-M port allocates request+128 system bytes:4224 each for Motion,
Network and Observer,8320 for Decoder; total20992 bytes inside the pool.
`td_ref_tsk` locates the extent while the task is dormant. Its top128bytes are
left untouched to preserve the initial CPU context. The remainder is painted
0xa5 before `tk_sta_tsk`, never repainted after starting. The observed watermark
includes the unpainted128bytes conservatively.

Observer scans with interrupts/preemption enabled, once per second. Only short
native metadata copies use IRQ locks; encoding and transmission happen outside
them. A changed low16-byte marker requests the existing latched initialization
stop. This is diagnostic detection, not an MPU boundary or guaranteed overflow
prevention. Preemption and values coincidentally equal to0xa5 limit watermark
interpretation. The pinned port's task system-stack observations can include IRQ
use, without separately measuring an ISR-only bound. Kernel-pool free space and
unexecuted paths remain unmeasured. No stack was downsized.

## Binary fields and representations

Topic `inf2004/aai10/robot1/v1/runtime_diag`, paced at one row per minimum250ms,
round-robin across summary and four tasks at QoS0. A blocked row retries without
advancing; motion/status traffic has priority. Even QoS0 needs a request handle,
so these rows must not form a status burst. Robot ACL permits publication; observer credentials cannot
forge telemetry. USB emits `[RUNTIME_V1] <hex>`. Existing schemas stay unchanged.
Fields are LSB-first with zero padding. Common header: kind8/run16/capture-ms32.

| Record | Bytes | Fields after the seven-byte header |
| --- | ---: | --- |
| kind0 summary |36| armed1/source2/reason3/pad2; stop-sequence, request-us, disabled-us, armed guard-gap max, armed service-age max, monitor-work max, stack-scan count:32 each |
| kind1 task |34| ID2/priority5/valid1; requested14/allocated14/marker-prefix14/guard-intact1/observed1/pad4; iterations, lifetime gap/work max, armed gap/work max:32 each |

IDs0..3 mean Motion/Decoder/Network/Observer; source0..3 means boot/no event,
stop-API entry, guard observation, output rejection. Reasons retain0..6.
Stack dimensions are below16384 and priorities1..16; counters/timestamps retain
their32-bit domains. Clock differences use modulo32 within the measured window.
New runs clear armed maxima; old preempted spans cannot contaminate a new run.
MQTT records are scoped by status boot ID. USB has no boot ID and scopes each
reconnection separately. Older delayed records remain in raw logs but do not
replace newer summary values.

## Software evidence

`tools/test_rtos_runtime.py` executes actual observer/measurement/report code
with sanitized kernel/SDK call mocks. It checks creation attributes, dormant
context preservation, pre-start painting, creation/reference/start cleanup,
watermarks, low-marker stop, idle/armed separation, run reset, stale spans,
wrap, bounds/padding and independent Python decode. These are not physical
watermark or timing observations.

Board/runtime regressions verify API timing includes simulated lock delay and
both PWM writes across wrap, plus guard/service and rejection sources. Existing
start/stop/deadline/token and actual ADC-to-PWM A/A/Z recovery checks pass.
Runtime packet delivery, explicit lengths, existing scan export, backpressure,
reconnect and role ACLs pass through local real brokers. USB/MQTT console tests
cover v0.5 identity, saved observations and reconnection namespaces.

The ARM build uses `-Wall -Wextra -Werror -fstack-usage`. UF2 reconstruction is
compared to the linked binary. Compiler `.su` values describe individual frames,
not full call chains or runtime watermarks. See the JSON report for hashes and
resource figures. Task startup and idle observations have passed the Pico check;
loaded track timing remains pending.

## Initial no-track check (completed)

1. Connect the Pico to the Mac normally by USB, with battery disconnected and
   both powered wheels raised. No BOOTSEL or GP20 press.
2. The agent checks the broker IP and, if changed, rebuilds with saved local
   settings. No password is needed in chat. Verify the candidate image hash.
3. Flash/verify using the existing USB picotool; Finder mounting is unnecessary.
   Retain v0.4.2 for rollback.
4. Confirm v0.5, idle/stop-latched/no fault, then collect observations for all
   four tasks with their expected allocations and intact markers. Keep motors
   stopped. Verify data and idle MQTT STOP acknowledgement; save real results.
5. Update installed-image metadata only after successful installation/check.

The real idle observations show 548/224/1256/1120 bytes of watermark use for
Motion/Decoder/Network/Observer, with all low markers intact. These include the
unpainted128-byte context allowance and are not loaded stack limits. A138633us
lifetime Motion gap occurred during stopped startup. Armed maxima are zero
because no run occurred, not proof of zero latency.

Next checks use normal motor power with raised wheels for GP20/USB/MQTT abort,
then a real A/A/Z pass for loaded Decoder observations. A bounded move completing
before a STOP arrives is completion, not a demonstrated moving abort. No motor run
has been started by the agent. First select `f`, start through a GP20 press/release
and press GP20 again promptly while the wheels move. Check that release does
not restart them, wait five seconds for paced rows, then q/Enter. Keep both
powered wheels raised throughout.

## First moving GP20 check (completed)

[Saved acceptance evidence](rtos-v0_5-gp20-stop-acceptance.json) covers session
`rtos-network-20261109-100143`. The user reports prompt wheel stopping. USB and
MQTT agree on guard-origin user stop, aborted/latched state and fault0. The
guard observation to both PWM-off writes took6us. Armed Motion entry gap
peaked at10023us, motor-service age10140us and guard spacing5011us. These are
observations from one short raised-wheel run, not worst-case bounds or
mechanical braking measurements. All four low-stack markers remained intact;
74 subsequent MQTT motion records stayed aborted/latched, with unchanged
encoder counts across14690ms of captured device time. Decoder was idle in
this bench mode. The exit MQTT STOP acknowledged an already-stopped robot.

## USB/MQTT raised-wheel checks (completed)

[Saved acceptance evidence](rtos-v0_5-remote-stop-acceptance.json) covers session
`rtos-network-20261109-101245`. Runs2 and3 were active before their USB `x`
and MQTT `m` stops; both aborted/latched with fault0. Each stop API entry to
both PWM-off writes took11us, excluding command transport and host delay.
Armed Motion entry-gap maxima were10025us and10048us; motor-service ages
were10140us and10177us. All four tasks had intact low markers and matching
armed measurements over USB/MQTT. The broker restart while stopped recovered
telemetry with the same Pico boot ID, unchanged encoder counts and the latch
retained. This does not exercise armed network loss or loaded decoding.

The next physical step is the [same-image loaded A/A/Z track check](rtos-v0_5-track-check.md).
The completed bench procedure is retained below for reproducibility.

Keep USB and normal motor power connected, with both powered wheels raised.
Run `sh tools/run_rtos_network.sh --reuse` and wait for USB identity and valid
MQTT motion data. Enter each command letter followed by Enter.

1. **USB abort:** select `f`, start with a GP20 press/release, then promptly
   enter `x` while the wheels move. Expect `aborted`, stop latched and fault0.
   Check the wheels stay stopped; wait five seconds for runtime rows.
2. **MQTT abort:** select `f` again, start with GP20, then promptly enter `m`
   while moving. Expect a STOP acknowledgement and the same stopped state.
   Check the wheels stay stopped; wait five seconds.
3. **Stopped reconnection:** while stopped, enter `b`. Wait for broker restart,
   Pico online and valid motion data to return. Do not press GP20 during this
   check; the stop latch must remain set. Wait five seconds, then enter `q` to
   save and exit.

If either move completes before its command arrives, record completion rather
than a demonstrated moving abort. GP20 remains the physical stop throughout.
Stop the checks on a fault or failed physical stop and retain that session.
The broker restart here covers stopped reconnection, not armed network-loss
timing. A subsequent A/A/Z track pass will exercise the real Decoder paths.
