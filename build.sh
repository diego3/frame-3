#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

RAYLIB_VERSION="6.0"
RAYLIB_PATH="$SCRIPT_DIR/vendor/raylib"

ENTT_VERSION="v4.0.0"
ENTT_PATH="$SCRIPT_DIR/vendor/entt"

if [ ! -d "$RAYLIB_PATH" ]; then
    git clone --depth 1 --branch "$RAYLIB_VERSION" https://github.com/raysan5/raylib "$RAYLIB_PATH"
fi

if [ ! -f "$RAYLIB_PATH/src/libraylib.a" ]; then
    make -C "$RAYLIB_PATH/src" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
fi

if [ ! -d "$ENTT_PATH" ]; then
    # Header-only; no build step needed.
    git clone --depth 1 --branch "$ENTT_VERSION" https://github.com/skypjack/entt "$ENTT_PATH"
fi

# mini-yaml (docs/adr/0008): Engine::Init() now calls LoadOrCreateEngineConfig() (docs/adr/0011),
# which reuses ADR-0008's YamlEntityFileParser -- the main product build needs it too now, not
# just the test build. No release tags exist upstream, so this pins a commit instead (confirmed
# via `git ls-remote --tags` -- empty); a full clone + checkout, not --depth 1, since a shallow
# clone can't reliably target an arbitrary commit SHA the way --depth 1 --branch <tag> does above.
MINI_YAML_COMMIT="22d3dcf5684a11f9c0508c1ad8b3282a1d888319"
MINI_YAML_PATH="$SCRIPT_DIR/vendor/mini-yaml"

if [ ! -d "$MINI_YAML_PATH" ]; then
    git clone https://github.com/jimmiebergmann/mini-yaml "$MINI_YAML_PATH"
    git -C "$MINI_YAML_PATH" checkout "$MINI_YAML_COMMIT"
fi

# GAME selects which src/game/<GAME>/ module gets built (docs/rfc/0001-flare-reactor-pipeline-
# experiment.md, docs/adr/0017) -- defaults to sandbox, same as the Makefile's own default; export
# GAME=flare_reactor or GAME=camera_fps before calling this script to build one of the others
# instead.
GAME="${GAME:-sandbox}"

make -C "$SCRIPT_DIR/src" PLATFORM=PLATFORM_DESKTOP BUILD_MODE=RELEASE GAME="$GAME" RAYLIB_PATH="$RAYLIB_PATH" ENTT_PATH="$ENTT_PATH" MINI_YAML_PATH="$MINI_YAML_PATH"

# The Makefile always links build/desktop/raylib_game (its own GAME_MARKER/check_game relink-
# detection logic is keyed on that one fixed name never changing across GAME switches -- see
# src/Makefile's own comment on GAME). This copy, named after GAME, is purely a local-dev
# convenience so build/desktop/ holds a same-named-as-GAME executable too -- run.sh reads
# GAME_MARKER (written by the make invocation above) to know which one to launch. Lives here, not
# in the Makefile, so it never touches the release CI workflows (build_linux/macos/webassembly.yml
# call the Makefile directly and only ever reference $(PROJECT_NAME)).
BUILD_DIR="$SCRIPT_DIR/build/desktop"
cp -f "$BUILD_DIR/raylib_game" "$BUILD_DIR/$GAME"
