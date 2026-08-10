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

# Versioned identifier copy (2026-08-08): same "local-dev convenience, lives here not in the
# Makefile" reasoning as the $GAME copy above -- an *additional* stamped artifact, not a
# replacement for it. run.sh still launches build/desktop/$GAME unchanged; this one only exists so
# a binary sitting in build/desktop/ (or copied elsewhere) can be identified after the fact: which
# VERSION and which exact commit it was built from. VERSION (repo root) is a plain semver string,
# bumped by hand on deliberate milestones -- nothing here bumps it automatically. The git SHA is
# what actually answers "is this the build with my latest changes", which was the real question
# behind the flare_reactor/raylib_game staleness mix-up this replaces.
VERSION="$(cat "$SCRIPT_DIR/VERSION")"
GIT_SHA="$(git -C "$SCRIPT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)"
if ! git -C "$SCRIPT_DIR" diff --quiet 2>/dev/null || ! git -C "$SCRIPT_DIR" diff --cached --quiet 2>/dev/null; then
    GIT_SHA="${GIT_SHA}-dirty"
fi
VERSIONED_NAME="${GAME}-v${VERSION}-${GIT_SHA}"

# Drop this GAME's older versioned copies first -- otherwise build/desktop/ accumulates one ~2MB
# binary per build forever. Only ever one canonical versioned copy per GAME at a time, matching
# $BUILD_DIR/$GAME's own "always the latest" semantics.
rm -f "$BUILD_DIR/$GAME"-v*
cp -f "$BUILD_DIR/raylib_game" "$BUILD_DIR/$VERSIONED_NAME"
echo "Built $BUILD_DIR/$VERSIONED_NAME"
