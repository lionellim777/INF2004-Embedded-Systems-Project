# v0.5.1: use the latest crossing during barcode recovery

Branch `yixuan/rtos-barcode-recross`, based on the verified v0.5 runtime branch.
The candidate is built and software-tested; installation and a new physical
full-A recovery pass remain pending. The installed v0.5, first-passing v0.4.2
and accepted v5.63 images are preserved. See the
[candidate build/replay report](rtos-v0_5_1-recross-fix.json).

## What the loaded v0.5 run establishes

Session `serial-output/rtos-network-20261109-101959` decoded half A at71686ms
and bend Z at84940ms. Full A was rejected; this is a partial result, not AAZ
acceptance. The first failed capture was downloaded completely and checksum
verified:7746 words/3873 readings, FNV-1a`43be51a9`. Its recovery counter rose
from0 to2 in observed states. All samples and odometry remain available in
the committed binary fixture. See [the loaded observations](rtos-v0_5-loaded-track-observations.json).

The scanner kept sampling: ADC/queue fault bits0, queue peak30, overall
recoveries37. During this loaded run, armed Motion entry gap peaked at10079us,
motor-service age10426us and guard spacing5036us. All task low-stack markers
remained intact. Observed watermarks were Motion848B, Decoder1408B,
Network1256B and Observer1056B, including the untouched-context allowance.
Decoder elapsed work peaked at60595us including preemptions, while the
higher-priority Motion task continued. These observations do not establish
worst-case timing or justify resizing stacks. Arrival gaps reached14.31s;
transport delay must remain separate from local motor-service timing.

## Why full A failed

During correction, GP26's projected position backed up by186 counts, about
13.79mm under nominal odometry, then crossed the same region again. The
existing monotonic envelope retained the *first* observation at each forward
position. In this capture that joined an early filtered optical bump during
the first crossing to data after the repeated crossing. High-prominence
profiles therefore contained33 extrema instead of a complete frame's31.
The ordinary strict Code39 decoder correctly rejected that assembled trace.

The new envelope selects the *latest* monotonic traversal of the measured
coordinates. A reverse pass through positions marks strict suffix minima in
a1024-byte per-instance bitmap; reading those marked samples forward provides
the latest crossing. This selection depends only on measured positions,
before any letter decoding. It retains the raw ADC/time/position samples,
does not edit inferred edges, and keeps all existing guards, width models,
quiet-zone requirements and ambiguity rejection. The scan is linear in the
number of stored samples, with no allocation or interrupt masking.

`BARCODE_WAVE_LAST_PASS` defaults to0 for legacy builds. Only RTOS enables it.
The saved full-A input now has31 extrema at the three higher prominence
profiles and decodes A in both batch and captured-cadence streaming replay.
The sentinel first snapshot word seeds the previous level; it is not replayed
as a zero-time duplicate ADC callback. Sampling-gap rejection remains intact.

## Validation and resource cost

- Legacy, earlier RTOS and new RTOS modes all pass their existing A–Z,
  orientation, contrast, size, noise, partial-frame, guard, quiet-zone and
  timestamp-wrap regressions under ASan/UBSan.
- 104 synthetic A–Z/orientation/full-half recross cases recover only with the
  new selection.208 extra-mark-without-recross/missing-guard cases reject in
  every mode. Same-path recross, pauses, long-bar lateral shifts and leaving
  the printed band retain their existing checks.
- Archived half-Z/bend-Z inputs and the prior physical AAZ run's saved Z still
  decode correctly. These are replays, not new physical passes.
- Actual ADC queue/course/decoder/PWM host integration, motor start/stop/fault
  regressions, console/bench identity gates and local broker/setup checks pass.
- ARM `-Wall -Wextra -Werror -fstack-usage` build passes. Text451248B,
  BSS207332B, data0. BSS increases exactly1024B over v0.5; stack requests and
  the32KiB kernel pool are unchanged. UF2 blocks reconstruct the linked binary.
- Runtime, scan and motion wire schemas are unchanged. Helpers accept both
  installed v0.5 and candidate v0.5.1; new provisioning targets the latter.

## Next physical action

The previous capture is saved, so its power can now be removed. With the car
stopped, motor battery disconnected and both powered wheels raised, connect
the Pico directly to the Mac by micro-USB normally. No BOOTSEL or GP20 press.
Install/verify v0.5.1 over USB and check idle state, fault0 and all stack rows
before updating private installed-image metadata. No motor start is sent.

After that check, repeat the [wireless track procedure](rtos-v0_5-track-check.md)
using v0.5.1. No Mac cable is attached during travel. One physical run is enough
for the next analysis. Full-A recovery acceptance and course repeatability
remain open until actual runs establish them. Teammate terrain, obstacle and
command-execution modules remain untouched.
