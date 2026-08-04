#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DOCTEST_VERSION="v2.5.3"
DOCTEST_PATH="$SCRIPT_DIR/vendor/doctest"

if [ ! -d "$DOCTEST_PATH" ]; then
    # Header-only; no build step needed.
    git clone --depth 1 --branch "$DOCTEST_VERSION" https://github.com/doctest/doctest "$DOCTEST_PATH"
fi

make -C "$SCRIPT_DIR/src" tests DOCTEST_PATH="$DOCTEST_PATH"
"$SCRIPT_DIR/src/tests_runner"
