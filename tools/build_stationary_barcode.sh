#!/bin/sh
set -eu
ROBOT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export PICO_SDK_PATH="$ROBOT_ROOT/build/dependencies/pico-sdk"
export PICO_TOOLCHAIN_PATH="$ROBOT_ROOT/build/dependencies/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi"
export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
ROBOT_CMAKE="$ROBOT_ROOT/build/dependencies/venv/bin/cmake"
python3 "$ROBOT_ROOT/tools/embed_calibration_page.py"
"$ROBOT_CMAKE" -S "$ROBOT_ROOT/firmware/app/robot_firmware" -B "$ROBOT_ROOT/build/barcode-stationary" -G Ninja \
  -DPICO_BOARD=pico_w -DROBOCAR_WIRELESS_CALIBRATION=ON -DROBOCAR_STATIONARY_BARCODE=ON \
  -DCMAKE_MAKE_PROGRAM="$ROBOT_ROOT/build/dependencies/venv/bin/ninja" \
  -Dpicotool_DIR="$ROBOT_ROOT/build/encoder-inspection/_deps/picotool" \
  -DCMAKE_BUILD_TYPE=Release '-DCMAKE_C_FLAGS=-mcpu=cortex-m0plus -mthumb -Wall -Wextra -Werror'
"$ROBOT_CMAKE" --build "$ROBOT_ROOT/build/barcode-stationary" --parallel 8
cp "$ROBOT_ROOT/build/barcode-stationary/robot_firmware.uf2" "$ROBOT_ROOT/build/barcode-stationary/robocar_barcode_stationary_v5_7.uf2"
