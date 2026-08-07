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

# mini-yaml (docs/adr/0008): entity_file_parser_yaml_test.cpp/level_loader_test.cpp/
# engine_config_test.cpp need it (build.sh vendors it too now -- Engine::Init() calls
# LoadOrCreateEngineConfig(), docs/adr/0011, which reuses this same parser). No release tags exist
# upstream, so this pins a specific commit instead (confirmed via `git ls-remote --tags` -- empty)
# -- a full clone + checkout, not --depth 1, since a shallow clone can't reliably target an
# arbitrary commit SHA the way --depth 1 --branch <tag> does for raylib/EnTT above. The repo is
# small, so the extra history costs nothing that matters here.
MINI_YAML_COMMIT="22d3dcf5684a11f9c0508c1ad8b3282a1d888319"
MINI_YAML_PATH="$SCRIPT_DIR/vendor/mini-yaml"

if [ ! -d "$MINI_YAML_PATH" ]; then
    git clone https://github.com/jimmiebergmann/mini-yaml "$MINI_YAML_PATH"
    git -C "$MINI_YAML_PATH" checkout "$MINI_YAML_COMMIT"
fi

make -C "$SCRIPT_DIR/src" tests DOCTEST_PATH="$DOCTEST_PATH" RAYLIB_PATH="$RAYLIB_PATH" ENTT_PATH="$ENTT_PATH" MINI_YAML_PATH="$MINI_YAML_PATH"
"$SCRIPT_DIR/build/test/tests_runner"
