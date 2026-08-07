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

# NVIDIA=1 forces the run onto the discrete GPU on hybrid-graphics (Optimus) laptops via PRIME
# render offload. Verified 2026-08-07: on this machine (prime-select mode "on-demand"), a bare
# launch renders on the Intel iGPU (raylib's startup log prints "Vendor: Intel" and the process
# never shows up in `nvidia-smi`); these two env vars are what put NVIDIA on both instead.
if [ -n "${NVIDIA:-}" ]; then
    export __NV_PRIME_RENDER_OFFLOAD=1
    export __GLX_VENDOR_LIBRARY_NAME=nvidia
fi

cd "$BUILD_DIR"
exec "./$GAME_TO_RUN"
