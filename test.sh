#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DOCTEST_VERSION="v2.5.3"
DOCTEST_PATH="$SCRIPT_DIR/vendor/doctest"

if [ ! -d "$DOCTEST_PATH" ]; then
    # Header-only; no build step needed.
    git clone --depth 1 --branch "$DOCTEST_VERSION" https://github.com/doctest/doctest "$DOCTEST_PATH"
fi

# hierarchy_test.cpp (docs/adr/0002) needs entt::registry and raylib's Vector3/Quaternion/Matrix
# headers -- both header-only for this purpose (raymath.h's functions are RMAPI/static inline), so
# only a checkout is needed here, not a full build.sh run (no libraylib.a to compile/link against).
RAYLIB_VERSION="6.0"
RAYLIB_PATH="$SCRIPT_DIR/vendor/raylib"

ENTT_VERSION="v4.0.0"
ENTT_PATH="$SCRIPT_DIR/vendor/entt"

if [ ! -d "$RAYLIB_PATH" ]; then
    git clone --depth 1 --branch "$RAYLIB_VERSION" https://github.com/raysan5/raylib "$RAYLIB_PATH"
fi

if [ ! -d "$ENTT_PATH" ]; then
    git clone --depth 1 --branch "$ENTT_VERSION" https://github.com/skypjack/entt "$ENTT_PATH"
fi

make -C "$SCRIPT_DIR/src" tests DOCTEST_PATH="$DOCTEST_PATH" RAYLIB_PATH="$RAYLIB_PATH" ENTT_PATH="$ENTT_PATH"
"$SCRIPT_DIR/src/tests_runner"
