# Repository Guidelines

## Project Structure & Module Organization

`src/` contains the application. Keep engine-facing C++ code in `src/app/` (engine, events, processes, loading, configuration) and screen-oriented game code in `src/game/`. Platform packaging files live in `src/platform/`. Unit tests are in `src/tests/`; each test source targets one focused component. Game content belongs in top-level `assets/`, never under `src/`. Third-party dependencies are vendored in `vendor/`; do not edit them unless deliberately updating a dependency. Architectural decisions and planned work are recorded in `docs/adr/` and `docs/roadmap.md`.

## Reference Implementation

Use the local `/home/diego/Documents/gamecode4` checkout as the preferred *Game Coding Complete* reference; use [MikeMcShaffry/gamecode4](https://github.com/MikeMcShaffry/gamecode4) when the local clone is unavailable. Consult `Extra/UtilityDemo` for compact Lua and utility-AI examples, and `Source/TeapotWars` for full application integration (game logic, views, events, input, loading, physics, and networking). It is maintained more recently than the book's printed code; when they differ, inspect the repository's current implementation and adapt it to this project's raylib/C++20 architecture rather than copying it blindly.

## Build, Test, and Development Commands

- `./build.sh` — fetches required dependencies when absent, then builds the desktop game.
- `./run.sh` — starts the built `src/raylib_game` executable.
- `./build-run.sh` — builds and immediately runs the game.
- `./test.sh` — fetches test dependencies if needed, builds `src/tests_runner`, and executes all unit tests.
- `make -C src clean` — removes local build outputs before a clean rebuild.

For direct builds, use `make -C src PLATFORM=PLATFORM_DESKTOP` and pass explicit dependency paths when they are not in `vendor/`. Web builds use `PLATFORM=PLATFORM_WEB` with Emscripten available on `PATH`.

## Coding Style & Naming Conventions

Follow `CONVENTIONS.md`. Use four spaces, no tabs or trailing whitespace, initialize every variable, and place braces on aligned lines. C/C++ variables and members use `lowerCase`; types, enums, and functions use `TitleCase`; macros and enum members use `ALL_CAPS`. Name files and directories `snake_case` without spaces, for example `entity_file_parser_yaml.cpp` and `assets/characters/player.png`.

## Testing Guidelines

Tests use doctest and are named `*_test.cpp` under `src/tests/`; keep assertions focused on observable behavior. Add or update tests alongside changes to engine, parsing, configuration, or game-logic code. Run `./test.sh` before opening a pull request. There is no stated coverage threshold; maintain coverage for new branches and regressions.

## Commit & Pull Request Guidelines

Recent history favors concise imperative subjects, such as `Implement ADR-0009 (...)`, `Add ADR-0011: ...`, and `Fix stale roadmap cross-reference ...`. Keep each commit scoped to one change. Pull requests should describe the behavior and affected ADR/roadmap item, link relevant issues when available, include test results, and attach screenshots for visible game or UI changes. Avoid committing generated binaries, object files, or build directories.
