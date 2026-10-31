# v5.16: first real-track corner correction

## Evidence

User log: `/Users/cyix/Downloads/AAI10-motion-2026-10-29T06-25-55-795Z.txt`.
Two v5.15 CONTINUOUS_SCAN runs, not the Y/Z mission. Both ended in
CONTINUOUS_LINE_AMBIGUOUS, with zero invalid encoder transitions and zero
barcode reads. During a sustained left edge, the target stayed at 900/1200
counts/s; then both-white restored near-equal forward targets. User observed
understeering off a 90-degree strip. The log has no measured heading and cannot
establish the exact point the vehicle left the track. Both-black at the final
stop does not prove a junction or cliff.

## Changes

Continuous scan and Y/Z mission share a provisional 900 counts/s forward target
(nominal 66.7 mm/s at 2543 CPR and 60 mm wheels). One-sided black immediately
caps the inside target at 250 counts/s; sustained same-side black for 80 ms
sets that target and its PWM to zero. This removes drive, not a measured active
braking guarantee. A strong correction lasts at most 100 ms after last seeing
the edge, then acceleration ramps back up at 4000 counts/s/s. The outside
wheel is never boosted above the forward target. Opposite edge replaces the
correction. Both-black crossing and committed centering clear corner memory.

Speed feedback continues. Deliberate zero-target inside-wheel stops refresh
only that wheel's stall grace; the outside wheel's 1-second stall protection
remains. Restoring a nonzero target restores stall monitoring. Existing stop,
sampling, timing, travel and continuous both-black limits remain.

Single-barcode diagnostic remains at 1200 counts/s with its previous steering.
Standalone 30 cm and 90/180-degree manoeuvre targets are unchanged. Y/Z mapping,
replacement/deferral/Stop rules and barcode decoding are unchanged.

## Verification

- Strict Pico W build and UF2 RP2040 headers, family, address range and version.
- Sanitized host suites: both correction directions, zero inside power,
  bounded hold, opposite edge, outside-wheel stall, crossing precedence,
  all existing capture/decoder and motion fault tests.
- All 16 production course main-loop synthetic scenarios passed, including
  immediate/deferred turns and Stop preemption.
- Wireless and mocked browser tests recorded in build/wireless-host-tests.
- Prior v5.15 UF2 preserved. No hardware flashed or moved by the agent.

These are software checks. Sharp corner tracking, optical contrast, traction
and post-change barcode performance require physical validation.

## Next run

Flash v5.16, reload page, set Y and Z to Go straight, Select Y/Z mission and
confirm the selection log. Start 20-30 cm before the same corner with GP20.
Stop after negotiating it (or immediately if it leaves the strip), then download
the log. Observe successful tracking, understeering, or overcorrection/wobble.
Do not change sensor mounting and firmware parameters simultaneously.
Both-white still cannot reliably distinguish centred from missing line; the
100 ms memory is not full line-loss recovery or guaranteed 90-degree handling.
