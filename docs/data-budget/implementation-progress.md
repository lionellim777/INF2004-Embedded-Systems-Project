# Minimum-bit implementation progress

The 1,352-byte figure is the target allocation for a proposed set of application
records and buffers. It is not the complete firmware RAM usage, and there is no
1,352-byte block to upload. Firmware changes must replace the existing records
and their producers/consumers, then be compiled into the car's firmware.

## Step 1: lossless barcode timing storage — coded, host-tested

Changes on `codex/minimum-bit-budget`, 21 October 2026:

- `barcode_runs.h` stores 29 timings at 20 bits each in 73 bytes, instead of
  29 four-byte integers (116 bytes). Accepted values remain 0..750,000 us.
  Zero retains its previous capture semantics and is rejected as a character
  element by the decoder. Values above the timeout cannot be stored.
- `barcode_reader_t` uses those packed timings. Reordering its fields avoids
  extra alignment gaps: the Cortex-M0+ type size changes from 132 to 88 bytes,
  a 44-byte reduction per reader instance. This is a type-size measurement,
  not a measured change in whole-program BSS or peak stack consumption.
- The decoder reads packed timings directly; it does not create a second
  unpacked 116-byte capture array. Its existing nine-element sorting scratch
  and other temporaries are not yet compressed.
- Existing A–Z support, forward/reverse decoding, microsecond precision,
  750 ms edge timeout, three-second duplicate suppression and 32-bit clock
  wrap handling are retained. Packed storage has one owner; it is not an
  atomic cross-task interface.

The proposed **82-byte** reader is still a later target. It also packs metadata
and relies on different timestamp/session rules. Those rules have not been
implemented. This first change intentionally retains native timestamp fields.
The full 1,352-byte target therefore remains a proposal.

Checks completed:

- Existing barcode/navigation host regression passed.
- AddressSanitizer and UndefinedBehaviorSanitizer passed: 10,000 random field
  updates with neighbouring fields checked, boundary values at every position,
  reserved values, invalid writes and unused padding checks.
- All 26 letters in both directions at three scales passed (156 cases).
- 5,000 additional corrupted/random captures matched the original decoder at
  commit `692f6ed6f278e282541bea0c2ded680693c4f24a`.
- Reader tests passed for clock wrap, duplicate expiry, timeout boundary and
  abort/re-arm behaviour.
- Cortex-M0+ compile-time assertions confirmed 73-byte capture storage and an
  88-byte reader. The decoder compiled for that ARM target.

Reproduce from the repository root:

```sh
python3 tools/test_barcode_storage.py
```

On this Mac the default MacOSX27 SDK fails to link with the installed linker.
The existing MacOSX26.5 SDK worked, without changing system settings:

```sh
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk python3 tools/test_barcode_storage.py
```

**Update, 21 October:** an isolated Pico SDK/Arm GNU/CMake environment is now
available under `/private/tmp/inf2004-pico-build`. The full main application
build passed and includes packed barcode capture plus the ported Batch 5
motion controller. See [motion integration](../motion-integration.md) for the
actual new build and first floor check. No fresh physical integration results
are claimed. Completed encoder/sonar/servo calibrations remain the baseline;
do not repeat them solely to deploy compact storage.

## Remaining implementation sequence

1. **Check the compiled integration build on the car.** The Pico build now
   works. Compare actual barcode/control behaviour with the existing baseline
   and retain the source-built baseline firmware as a recovery option.
2. **Convert the other sensor storage.** Pack ADC samples and raw IMU history;
   retain arithmetic precision until quantified. Bring in Buddy 5's distance
   plus validity representation. Test min/max, invalid and stale readings.
3. **Finish the motion and shared-data contracts.** Agree/enforce speed, run
   duration, count ranges and reset signalling before shrinking counters.
   Preserve raw ISR counts and define an ISR-safe snapshot. Do not silently
   introduce the proposed 30-minute stop policy. Resolve packed-field owners,
   GPIO interrupt dispatch and the six-required/four-configured mutex limit.
4. **Integrate tasks and binary communications.** Port calibrated motion into
   the six-task runtime while retaining stop/lease/generation rules. Replace
   simulated producers with real owner snapshots. Implement the shared binary
   schema and matching laptop decoder; use explicit payload lengths rather
   than `strlen`. The proposed full snapshot is 69 bytes before network headers.
5. **Measure and tune the complete program.** Record linked data/BSS, task-stack
   high-water marks, kernel-pool/heap margins, message sizes, ISR cost and
   deadlines under load. Exercise overflow, resets, stale data, queue-full and
   network loss. Only then revise the pool sizes or claim an integrated total.

The 96 KiB kernel pool and SDK/network allocations are separate from the
1,352-byte target. Stacks and kernel message buffers allocated inside that pool
must not also be counted as extra static RAM.
