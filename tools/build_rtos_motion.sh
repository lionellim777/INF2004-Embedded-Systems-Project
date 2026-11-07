#!/bin/sh
set -eu
ROBOT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export PICO_SDK_PATH="${PICO_SDK_PATH:-$ROBOT_ROOT/build/dependencies/pico-sdk}"
export PICO_TOOLCHAIN_PATH="${PICO_TOOLCHAIN_PATH:-$ROBOT_ROOT/build/dependencies/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi}"
export MTK_BSP_PATH="${MTK_BSP_PATH:-$ROBOT_ROOT/build/dependencies/mtk3_bsp}"
if [ -d /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk ]; then
    export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
fi
ROBOT_CMAKE="${ROBOT_CMAKE:-$ROBOT_ROOT/build/dependencies/venv/bin/cmake}"
ROBOT_NINJA="${ROBOT_NINJA:-$ROBOT_ROOT/build/dependencies/venv/bin/ninja}"
"$ROBOT_CMAKE" -S "$ROBOT_ROOT/firmware/app/mission_rtos" -B "$ROBOT_ROOT/build/rtos-motion" -G Ninja \
    -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Release \
    -DPICO_NO_PICOTOOL=0 \
    -Dpicotool_DIR="${ROBOT_PICOTOOL_DIR:-$ROBOT_ROOT/build/encoder-inspection/_deps/picotool}" \
    -DCMAKE_MAKE_PROGRAM="$ROBOT_NINJA" \
    '-DCMAKE_C_FLAGS=-mcpu=cortex-m0plus -mthumb'
"$ROBOT_CMAKE" --build "$ROBOT_ROOT/build/rtos-motion" --parallel 8
"$PICO_TOOLCHAIN_PATH/bin/arm-none-eabi-size" "$ROBOT_ROOT/build/rtos-motion/robocar_rtos_motion.elf"
