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

# Engine::Engine() (app/core/engine.cpp) now does this same PRIME-offload trick itself by default
# on Linux desktop, whenever an NVIDIA glvnd vendor is actually installed -- so a bare run.sh (or
# even the raw binary, run.sh or not) already lands on the discrete GPU on hybrid-graphics
# (Optimus) machines. NVIDIA=1 here is now just an explicit, no-detection-needed way to force the
# same thing; the one job this block still does that the engine can't is NVIDIA=0, an opt-out --
# exporting __GLX_VENDOR_LIBRARY_NAME=mesa here wins over the engine's own default (it only sets
# that var when unset) and forces the run back onto the iGPU, e.g. for power-constrained testing.
case "${NVIDIA:-}" in
    1) export __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ;;
    0) export __GLX_VENDOR_LIBRARY_NAME=mesa ;;
esac

cd "$BUILD_DIR"
exec "./$GAME_TO_RUN"
