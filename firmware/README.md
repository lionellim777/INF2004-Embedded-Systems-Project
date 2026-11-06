# Firmware

## Application

`app/robot_firmware` contains the v5.63 Pico SDK calibration baseline: GP28 centre/GP27 right following, dedicated GP26 barcode decoding, encoder control, terrain monitoring, separate obstacle-bypass mode and local Wi-Fi diagnostics. Barcode decoding is physically validated on the current AAZ home route. The final combined mission and µT-Kernel/MQTT integration remain pending; see `../docs/project-next-steps.md`.

## Hardware tests

Each folder under `tests` is independently buildable and flashable. Keep these programs small and deterministic so they can isolate hardware faults during integration.

Do not copy tested code blindly into the application. Move stable behaviour behind a documented module interface and keep only one owner for each peripheral.

