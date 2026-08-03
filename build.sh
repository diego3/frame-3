#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

RAYLIB_VERSION="6.0"
RAYLIB_PATH="$SCRIPT_DIR/vendor/raylib"

if [ ! -d "$RAYLIB_PATH" ]; then
    git clone --depth 1 --branch "$RAYLIB_VERSION" https://github.com/raysan5/raylib "$RAYLIB_PATH"
fi

if [ ! -f "$RAYLIB_PATH/src/libraylib.a" ]; then
    make -C "$RAYLIB_PATH/src" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
fi

make -C "$SCRIPT_DIR/src" PLATFORM=PLATFORM_DESKTOP BUILD_MODE=RELEASE RAYLIB_PATH="$RAYLIB_PATH"
