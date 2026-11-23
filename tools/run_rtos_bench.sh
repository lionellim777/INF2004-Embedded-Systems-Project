#!/bin/sh
set -eu
ROBOT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ROBOT_PYTHON="$ROBOT_ROOT/build/dependencies/rtos-tools-venv/bin/python"
if [ ! -x "$ROBOT_PYTHON" ]; then
    echo "Run the project RTOS tools setup first; see firmware/app/mission_rtos/README.md"
    exit 1
fi
exec "$ROBOT_PYTHON" "$ROBOT_ROOT/tools/rtos_bench.py" "$@"
