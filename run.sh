#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/src/raylib_game"

if [ ! -x "$BIN" ]; then
    echo "Executable not found. Build it first with ./build.sh" >&2
    exit 1
fi

cd "$(dirname "$BIN")"
exec ./raylib_game
