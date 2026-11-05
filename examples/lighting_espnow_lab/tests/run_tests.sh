#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BIN=$(mktemp -t lighting-lab-test)
trap 'rm -f "$BIN"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/components/lighting_core/include" "$ROOT/tests/test_core.c" "$ROOT/components/lighting_core/lighting.c" "$ROOT/components/lighting_core/protocol.c" -lm -o "$BIN"
"$BIN"
python3 "$ROOT/tests/test_bridge.py"
