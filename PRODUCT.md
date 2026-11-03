# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Users
INF2004 group members testing an autonomous two-wheel robot car, standing beside the track with a phone or laptop joined to the car's `AAI10-Calibrate` Wi-Fi access point. They glance between the car and the screen, so state must be readable in a second. Lecturers and assessors also see the page during demonstrations.

## Product Purpose
The on-board diagnostic page (served by the Pico W at http://192.168.4.1) selects a test, shows what the car is doing, shows decoded barcode values, and records a detailed log that lets the team pinpoint why a run stopped. Success means a failed run can be explained from the page without reading raw firmware source.

## Operating Context
- Browser selects and prepares tests only; the physical GP20 button starts and aborts. Web Stop needs Wi-Fi.
- The page polls `/logs?since=` every 500 ms and `/result` when idle; headers `X-Boot`, `X-Next`, `X-Gap`, `X-Idle` carry state.
- Live data comes only from tagged firmware log lines (`[AVOID]`, `[BARCODE EVENT]`, `[BARCODE PARTIAL]`, `[MOTION STOP]`, `[MOTION SETTLED]`, `[TIMING]`, `[CHECK]`, etc.). There is no periodic telemetry stream; firmware is not to be changed for UI work.
- Downloaded logs are shared in chat for analysis.

## Capabilities and Constraints
- Single self-contained HTML file embedded in flash via `tools/embed_calibration_page.py`; no external fonts, scripts or network assets (the phone has no internet on the car's AP).
- Must work equally on phone and laptop.
- Host browser test: `tools/test_calibration_page.cjs` (mock robot) must keep passing; it relies on existing element IDs and functions (`inspectOptical`, `opticalBins`, `downloadedResult`, `recover`, `resultPending`).
- Barcodes: Code 39, letters A–Z, framed `*X*`, 29 runs; Y/Z trigger configurable actions.

## Evidence on Hand
Recorded logs in `docs/testing/logs/`. No performance claims beyond what logs show; physical v5.55 validation is pending.

## Product Principles
1. The car's state is legible at arm's length.
2. Every stop shows its reason in plain words, with the raw evidence one tap away.
3. Never imply a result the firmware did not report.
4. The browser never starts motion; the UI keeps that boundary obvious.
