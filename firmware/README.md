# Firmware

## Application

`app/robot_firmware` contains the current integration milestone. It is not yet the final autonomous application and currently uses an early two-sensor line-following loop.

## Hardware tests

Each folder under `tests` is independently buildable and flashable. Keep these programs small and deterministic so they can isolate hardware faults during integration.

Do not copy tested code blindly into the application. Move stable behaviour behind a documented module interface and keep only one owner for each peripheral.

