#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build/desktop"
MARKER="$BUILD_DIR/.built_game"

# Which GAME to run: an explicit GAME=... env var wins; otherwise fall back to whichever GAME
# build.sh last actually linked (src/Makefile's own GAME_MARKER, written by check_game every
# build) -- so a bare ./run.sh always launches whatever you most recently built, even if that
# wasn't the sandbox default.
if [ -n "${GAME:-}" ]; then
    GAME_TO_RUN="$GAME"
elif [ -f "$MARKER" ]; then
    GAME_TO_RUN="$(cat "$MARKER")"
else
    echo "No build found and GAME not set. Build first with ./build.sh (or GAME=<name> ./build.sh)." >&2
    exit 1
fi

BIN="$BUILD_DIR/$GAME_TO_RUN"

if [ ! -x "$BIN" ]; then
    echo "Executable not found at $BIN. Build it first with GAME=$GAME_TO_RUN ./build.sh" >&2
    exit 1
fi

cd "$BUILD_DIR"
exec "./$GAME_TO_RUN"
