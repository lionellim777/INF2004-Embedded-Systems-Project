# Wireless calibration — v5.4, 28 October 2026

> Current next test: use the separate **v5.5 stationary diagnostic** on the short
> platform. [UF2 workflow and limits](testing/barcode-v5_5-stationary-scan.md).
> The v5.4 moving instructions below describe the previous build and are not the
> next recommended test. v5.5 has no driving commands.

## Current next step: moving barcode pass

Use **Barcode pass · A / Z**, then GP20, with large A beside a straight route.
GP26 is the offset barcode sensor; GP28/GP27 are the central line pair. Begin
with GP26 on white about 5 cm before the first bar. Allow at least 1 m of clear,
continuous level surface. A successful read stops and reports `decoded=A valid=1`.
Then test large Z, small A and small Z separately. Download each log; observations
remain in chat. Keep all sensor paths clear of tape/labels and all wheels away
from foam edges. Sonar cannot detect a drop-off.

v5.4 adds alarm-driven sampling and buffered edge timestamps to avoid the old
foreground acquisition gaps. It stops on a read, 8 s / 8000 counts, or a fault;
this is not full-course navigation. Existing motor targets remain unchanged.
See [the complete v5.4 procedure, changes and pending tests](testing/barcode-v5_4-moving-pass.md).

## v5.1 address-assignment fix

The first v5 on-car attempt joined the AP but the Mac reported a self-assigned
169.254 address. This was traced to a firmware configuration error: lwIP's DHCP
client was disabled (appropriate for AP-only operation), but the server-port
accept hook was missing. IPv4 input consequently rejected the client's initial
source address 0.0.0.0 before the application DHCP handler ran.

v5.1 defines `LWIP_IP_ACCEPT_UDP_PORT(port)` for network-order UDP port 67. The
new `tools/test_wireless_dhcp_lwip.py` compiles the actual pinned lwIP and real
DHCP server, with a simulated NIC/clock. It reproduces the packet drop with the
old configuration, then verifies DISCOVER/OFFER/REQUEST/ACK and the assigned
192.168.4.16 address with the corrected configuration, under ASan/UBSan.
Host memory alignment is adapted to 64-bit pointers; network options otherwise
match firmware. Earlier handler-level mocks did not cover this failure.

v5.2 retains this fix. The latest user log demonstrates working wireless runs and
completed turns on v5.1; the user reports that the 30 cm travel is also correct.
The original versioned v5/v5.1 UF2 files are retained for traceability.

## Where this continues

The user has confirmed 30 cm straight travel, both 90-degree turns and right180
on earlier firmware. v5.3 showed line correction and an intended both-black stop
on a perpendicular approach. Those results justify proceeding to moving barcode
acquisition; full-course junction/loss recovery and reliable decoding remain open.

## Flash and connect

1. With motor power off, hold the Pico's BOOTSEL button while connecting USB to
   the Mac. Copy `build/motion-wireless/robocar_motion_integration_v5_4_wireless.uf2`
   onto `RPI-RP2`, as with previous UF2 files. Keep the v5.3 file as a rollback.
2. Disconnect the Mac cable. Supply the car from your portable power bank using
   the existing USB power route. Secure the bank in the position intended for
   runs. Keep the car still and level during startup zeroing. Allow about 10 seconds.
3. On the Mac (or a phone), join **AAI10-Calibrate**, password **AAI10-drive**.
   This is a local diagnostic network with **no internet**; stay connected even if
   the operating system warns about that. No home/campus Wi-Fi credentials needed.
4. Open **http://192.168.4.1/** in a normal browser. Use `http`, not `https`.
   Wait for **Connected · car logs live** and live `Run=0` status.
5. Keep one page open. There is no Terminal process, server or serial extension
   to start for wireless operation. USB output still works with the existing
   VS Code serial monitor at 115200 if you reconnect USB.

The power bank's ability to supply motor-start current remains an on-car check.
If the car repeatedly restarts or its network disappears when the motors start,
stop the test and report that behaviour before changing motor calibration.

## Next run

Follow the [v5.4 large-A first procedure](testing/barcode-v5_4-moving-pass.md).
Confirm V5.4 on the refreshed browser, select Barcode pass, wait for SELECT, then
GP20. Wait for the settled and barcode result before lifting the car. Send the
downloaded log even if no letter was decoded. Do not repeat earlier motor
calibration unless the physical behaviour has changed.

## Connection and recovery behaviour

- GP20 is the only start command. Browser buttons only select a test or request a
  stop. Selections are rejected while moving or settling. No remote start route.
- Wireless boot defaults to the **stopped straight-30-cm selection**, so a power
  reset does not put GP20 into line-follow mode. USB `t`/`n` explicitly selects the bounded line check.
- Losing Wi-Fi does **not** automatically stop a run: the already selected bounded
  manoeuvre continues with its existing endpoint/fault/time limits and physical
  abort. Reconnecting does not start or repeat movement.
- The robot keeps the latest **16 KiB** of log in RAM and retains its latest stop
  and settled lines plus check/timing/barcode summaries separately. **Recover last result** retrieves those even if
  the rolling log has overwritten the run. A new motion or power loss clears
  these results; there is no flash log storage.
- The browser keeps up to 1 MiB while the page remains open and labels missing
  data, log trimming and power resets. Downloads include the captured log and
  the recovered result (which may duplicate lines in the live log). Download after
  each run. A browser refresh discards its accumulated log.
- The AP password is a shared diagnostic credential shipped in source; use this
  as a temporary local calibration network, not a public remote-control service.

## Implementation boundaries

- `wireless/calibration_core.*`: bounded log capture, cursor validation, retained
  result and stop-priority command mailbox, all owned by the foreground loop.
- `wireless/calibration_wifi.c`: three fixed HTTP client slots, 1536-byte request
  limits, 4096-byte log batches, five-second connection lifetime, one send chunk
  per service call. TCP partial requests, failed sends and disconnects are handled.
- The HTML/JS is stored in flash; no external libraries, internet or cloud service.
  `tools/embed_calibration_page.py` regenerates its C header.
- Pico SDK 2.1.1 poll-mode networking shares the main loop. Application handlers
  do not wait for clients. Driver execution time still needs measurement under
  real traffic. `[TIMING]` reports largest loop gap and Wi-Fi poll duration since the latest GP20 start. It is **not** a timing guarantee.
- Existing sonar reads remain bounded but blocking. No new 50 ms stop-latency
  guarantee, final RTOS integration or obstacle bypass is claimed.
- Diagnostics deliberately reserve RAM for readable logs and networking. The
  earlier **1352-byte application-record proposal is not this build's RAM total**.
  See `build/motion-wireless/build-report-v5_4.json` for this artifact's size/layout.
- v5.4 preserves all four v5.2 motion targets. USB and
  network stop processed together with GP20 cannot restart motion in that loop.

## Verification

Passed on the development computer:

- Motion-controller ASan/UBSan tests (existing targets, balancing, PI, endpoints,
  abort, encoder faults, timeouts and wrap handling).
- Barcode storage/pipeline regression and Cortex-M0+ layout checks.
- Wireless log/cursor/retained-result/command-priority tests under ASan/UBSan.
- Actual HTTP and DHCP handlers tested with mocked radio/lwIP: fragmented requests,
  missing control headers, moving-state rejection, invalid cursors, oversized and
  NUL-containing input, send-buffer exhaustion/recovery, connection cleanup,
  client limits, DHCP option bounds, 5000 malformed packets, offers/requests/renewals.
- Browser page with mock responses: selections, log/result download without a feedback form,
  moving-state lockout, disconnect/reconnect, reboot labels, desktop and mobile
  layouts, no JavaScript errors. Screenshots are fixtures, not physical evidence.
- Full Pico W compilation with `-Wall -Wextra -Werror`, then UF2 structure/family
  and banner checks.

Observed previously: successful wireless movement and v5.3 bounded line checks.
Pending on v5.4: physical sampling timing, moving decoding of all four sheets,
steering/coasting and simultaneous encoder/sonar/network behaviour. The agent has
not flashed or run the car. Run `python3 tools/test_barcode_calibration.py` for
the new acquisition and motion-protection host checks.

## Rebuild

Use the isolated toolchain in `build/dependencies` (ignored by Git): Pico SDK
2.1.1, Arm GNU 14.2.Rel1, CMake/Ninja. The checked-in source does not include SDK
binaries or personal network credentials. Run `python3 tools/embed_calibration_page.py`
if editing the HTML. Configure with `-DROBOCAR_WIRELESS_CALIBRATION=ON`;
`tools/build_wireless_calibration.sh` supplies the project-local toolchain paths.

Run `python3 tools/test_wireless_calibration.py` for host protocol tests.
`tools/test_calibration_page.cjs` uses Playwright via `NODE_PATH` and a headless
Chrome executable; it never connects to the real car. All browser endpoints are mocked.

The DHCP helper was adapted from Raspberry Pi pico-examples tag `sdk-2.1.1`,
`pico_w/wifi/access_point/dhcpserver/` (MicroPython MIT copyright retained).
Local modifications validate received option bounds/cookies, handle padding and
standard ciaddr renewals, check server selection and bind errors, and omit DNS
advertising because this build provides a numeric-address page only.
