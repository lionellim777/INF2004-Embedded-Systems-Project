#!/bin/sh
set -eu
ROBOT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec "$ROBOT_ROOT/build/dependencies/rtos-tools-venv/bin/python" "$ROBOT_ROOT/tools/rtos_network_session.py" "$@"
