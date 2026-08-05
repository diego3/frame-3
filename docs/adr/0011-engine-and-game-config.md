# 11. Game options: two-tier config (`EngineConfig` + `GameConfig`), writable, outside `assets/`

- Status: Accepted
- Date: 2026-08-04

## Implementation status (2026-08-05)

Landed as designed, with two additions beyond the ADR's own sketch:

- `EngineConfig`/`LoadOrCreateEngineConfig` — `src/app/engine_config.h`/`.cpp`. Reads via
  `YamlEntityFileParser` (ADR-0008) as decided; writes via a minimal hand-written emitter
  (`SerializeEngineConfig`, file-local to `engine_config.cpp`) scoped to exactly this struct's flat
  fields, as decided. A field missing from an existing file keeps its struct default rather than
  failing the load, using `EntityDefNode::TryGet` (not `Get`) throughout.
- `GameConfig`/`LoadOrCreateGameConfig` — `src/game/game_config.h`, header-only (not a `.cpp` the
  ADR's sketch implied): with zero fields there's nothing to parse, so it never needs ADR-0008's
  YAML machinery at all, just a file-existence check and an empty/comment-only file written on
  first run.
- **New shared utility, not named in the ADR**: `src/app/file_io.h`
  (`TryReadWholeFile`/`ReadWholeFile`/`WriteWholeFile`, header-only) — factored out once it became
  clear `LevelLoader` (ADR-0009) and `EngineConfig`/`GameConfig` both needed the same "read a whole
  file, creating parent directories on write" primitive, rather than duplicating it.
- `EntityDefNode` gained `AsBool()` (`src/app/entity_def.h`) — `EngineConfig::fullscreen` is the
  first real bool field this project's config/entity data has needed; mirrors mini-yaml's own
  `StringConverter<bool>` (case-insensitive `true`/`yes`/`1` vs. `false`/`no`/`0`).

**Wired into the real product, unlike ADR-0008/0009's pieces**: `Engine::Init()` now takes an
`EngineConfig` instead of raw `screenWidth`/`screenHeight` ints (`src/app/engine.h`/`.cpp`);
`Engine::Run()` uses `config.targetFps` for `SetTargetFPS`/`emscripten_set_main_loop`'s rate
argument, and the frame-budget-SLO warning threshold (`src/app/engine.cpp`) now tracks
`config.targetFps` too, not a fixed 60. `raylib_game.cpp`'s `main()` calls
`engine.Init(LoadOrCreateEngineConfig(), title)`, replacing the two hardcoded constants. Verified
end-to-end by hand: first run creates `config/engine.yaml` with the documented defaults; editing
`targetFps` in that file and re-running picks up the new value (confirmed via the frame-budget
warning's threshold changing to match).

**Not wired in**: `fullscreen`/`masterVolume` are loaded/saved but not yet applied to real
window/audio state (`ToggleFullscreen`, `SetMasterVolume`) -- the ADR's own phrasing ("or will")
treated these as fields to make room for, not a commitment to apply them in this pass; revisit once
a concrete need does. `GameConfig` has no caller anywhere in the codebase, exactly as decided.

**A build-system ripple beyond what ADR-0008 needed**: because `Engine::Init()` now calls
`LoadOrCreateEngineConfig()` for real, `entity_file_parser_yaml.cpp` and mini-yaml itself had to
move from test-only into the main product build (`PROJECT_SOURCE_FILES_CPP`, `build.sh`,
`ci_sanity.yml`'s "Build project" step) — previously only the test build needed them.

## Context

Nothing in this project loads game options from a file. `screenWidth`/`screenHeight` are
hardcoded `static const int`s in `app/raylib_game.cpp`, `main(void)` takes no `argc`/`argv`, and
`screen_options.c`'s `InitOptionsScreen`/`UpdateOptionsScreen`/`DrawOptionsScreen` are all `TODO`
stubs from the raylib template — there's no options UI and nothing behind one yet.
[`docs/roadmap.md`](../roadmap.md) tracks this as its own "Not started" item, distinct from the
separately-tracked input/key-binding system (action↔key mapping is a different problem from
display/audio settings).

*Game Coding Complete* loads a `PlayerOptions.xml` (or similar) early in startup — before the
window opens, since resolution/fullscreen have to be known before that call. The book is explicit
that this file lives **outside** the packaged resource system (the ZIP-based `ResCache` bundle
covered in Ch. 8/ADR-0004): options are something the player changes at runtime and the game has
to write back to disk, unlike art/sound/actor definitions, which are read-only shipped content. The
book also discusses (in its Windows-era context) not writing this file next to the game's
executable, since `Program Files` became non-writable for standard users from Vista onward,
preferring a per-user writable OS folder instead. That specific mechanism is Windows/2004-specific
and doesn't port; the *principle* — separate writable player config from read-only shipped content
— is exactly why this project already keeps `assets/` read-only-and-versioned (ADR-0004) and needs
a different, writable home for this.

A follow-up direction refined the scope further: the engine already has its own layering
(`Engine` owns only Ch. 5 Application-layer concerns, per [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md)
Decision 2 — it deliberately doesn't touch Game Logic+View) — so config should follow the same
split. Resolution/fullscreen/audio volume are literally parameters `Engine::Init()` already deals
with; whatever a specific game built on frame-3 wants to configure is not `Engine`'s business,
mirroring exactly why `Engine` doesn't own the screen state machine either.

## Decision

### Two config structs, two owners, matching the existing `Engine`/game boundary

```cpp
// app/engine_config.h (sketch) -- owned by Engine, since these are exactly the parameters
// Engine::Init() already takes/uses (window size, target FPS) or will (fullscreen, master volume).
struct EngineConfig {
    int screenWidth = 800;
    int screenHeight = 450;
    bool fullscreen = false;
    int targetFps = 60;
    float masterVolume = 1.0f;
};

// Reads config/engine.yaml (relative to cwd -- see "Where the file lives" below). If it doesn't
// exist yet (first run), returns EngineConfig{} (the defaults above) and writes them out to that
// path, so the file exists and is human-editable for next time -- the same "first run creates
// PlayerOptions.xml" pattern the book uses.
EngineConfig LoadOrCreateEngineConfig(const std::string &path = "config/engine.yaml");
```

```cpp
// game/game_config.h (sketch) -- owned by whatever specific game runs on frame-3, NOT by Engine,
// same reasoning ADR-0001 Decision 2 used to keep the screen state machine out of Engine.
// Deliberately near-empty: this raylib-template game has no game-specific settings yet (no
// difficulty, no gameplay toggles) -- this establishes the seam, not a schema nobody asked for.
struct GameConfig {
    // (nothing yet -- add fields here as a specific game needs them)
};

GameConfig LoadOrCreateGameConfig(const std::string &path = "config/game.yaml");
```

`Engine::Init()` changes from taking raw `(int screenWidth, int screenHeight, const char *title)`
to taking an `EngineConfig` plus `title` — `main()` calls
`engine.Init(LoadOrCreateEngineConfig(), "raylib game template")` instead of passing the two
hardcoded constants directly. `GameConfig` has no caller yet (nothing in this stub template needs
a game-specific setting) — it's loaded wherever/whenever a specific game's own code needs it, not
by `Engine` or by this ADR's own code.

### Where the file lives — outside `assets/`, next to `resources/`, not versioned

`assets/` is this project's read-only, version-controlled content root (ADR-0004) — options don't
belong there for the same reason the book keeps `PlayerOptions.xml` out of its ZIP bundle. Both
config files are generated at runtime (no shipped default file to version at all — the struct
defaults above *are* the shipped defaults), the same way `src/resources/` is a build-time-generated,
gitignored copy of `assets/` rather than a second source of truth. `config/engine.yaml` and
`config/game.yaml` live at `src/config/` (relative to the working directory the binary actually
runs from — the same cwd assumption `"resources/characters/mecha.png"`-style paths already rely on
elsewhere in `engine.cpp`), added to `.gitignore` as `/src/config/`, mirroring the existing
`/src/resources/` entry exactly.

### A new requirement ADR-0008 didn't need: writing YAML, not just reading it

[ADR-0008](0008-data-driven-entity-loading-yaml.md)'s `IEntityFileParser`/`EntityDefNode` only
ever need to *read* — entity/level definitions are shipped, read-only content. The "create the
file with defaults on first run" pattern needs the other direction too. Rather than building a
general "serialize any `EntityDefNode` back to YAML" capability (nothing else needs that yet), this
ADR proposes a minimal, hand-written emitter scoped to exactly the flat, scalar-fields-only shape
`EngineConfig`/`GameConfig` actually have (`key: value` lines, no nested maps/lists) — the same
"cover the actual need, not the general case" call ADR-0008 made for mini-yaml's partial spec
coverage.

### Depends on ADR-0008's parser landing in code — not on a PR chain this time

Unlike ADR-0009 (which had to branch off ADR-0008's still-open PR), ADR-0008 is now a merged ADR
document on `main` — this ADR can be written and reviewed independently. What it can't do is *ship
code* before `IEntityFileParser`/`EntityDefNode` actually exist (per `docs/roadmap.md`, ADR-0008 is
"Proposed... design not yet built"). This is an implementation-order dependency, not a git
dependency — tracked in `docs/roadmap.md` the same way ADR-0009's dependency on ADR-0008 already
is. If reusing ADR-0008's parser turns out to lag well behind this ADR's own priority, a
self-contained minimal YAML-subset reader scoped to this ADR's own flat structs is a legitimate
fallback (small, config-specific, not a competing general-purpose parser) — noted as an option, not
adopted by default, since duplicating parsing logic ADR-0008 already owns should be the last
resort, not the first move.

## Tradeoffs accepted

- No command-line override (`argv`) — explicitly out of scope for this ADR; `main(void)` keeps
  taking no arguments. Revisit only if a real need for CLI overrides (e.g. CI/automation launching
  the game with specific settings) shows up.
- No in-game options UI — `screen_options.c` stays exactly the stub it is today. This ADR makes the
  config *files* real; wiring a UI that reads/writes `EngineConfig`/`GameConfig` at runtime (so a
  player can change settings without hand-editing YAML) is separate, future work.
- `config/` is entirely unversioned/generated — accepted because there's nothing to ship a default
  for (the struct defaults are the shipped defaults); revisit only if a project ever wants to ship
  a non-trivial preset (e.g. a "recommended settings for Steam Deck" file) — not a need that exists
  today.
- Flat structs only, no nested/versioned schema — matches the "cover the actual need" reasoning
  used throughout every prior ADR in this project; revisit once `EngineConfig`/`GameConfig`
  actually need something structurally richer than a handful of scalars.
- `Engine::Init()`'s signature changes (from three positional args to `EngineConfig` + title) — a
  breaking change to the one existing call site (`raylib_game.cpp`'s `main()`), acceptable since
  it's a single caller in a project with no external consumers of `Engine` yet.

## Consequences / follow-ups

- `raylib_game.cpp`'s `main()` updates its one call site:
  `engine.Init(LoadOrCreateEngineConfig(), "raylib game template")` instead of the two hardcoded
  constants.
- `Engine::Run()` should use `EngineConfig::targetFps` instead of the hardcoded `SetTargetFPS(60)`/
  `emscripten_set_main_loop(..., 60, 1)` literals currently in `engine.cpp`, once this lands.
- `docs/roadmap.md`: move "Game options/config file" from "Not started" to "Proposed" pointing at
  this ADR once merged.
- `screen_options.c` is the natural future home for a real options UI reading/writing both configs
  — not built here, but this ADR is what makes that UI have something real to read and write.
- Once a second game/demo is built on frame-3 (per this project's own stated ambition — see
  ADR-0006's context, "a GTA-style 'vigilante' game, a Quake-style FPS"), that's the first real
  test of whether the `EngineConfig`/`GameConfig` split actually holds, and whether `GameConfig`'s
  currently-empty shape needs anything to make it useful in practice.

## Open Questions

- **Should the window title become part of `EngineConfig`?** Left as a separate `Engine::Init()`
  argument here (branding feels different from a "player option"), but worth revisiting once a
  second game/demo needs a different title than a hardcoded string.
- **`argv` overrides** — deferred (see Tradeoffs); revisit if a concrete automation/CI need for it
  shows up.
- **Packaged/distributed builds**: `src/config/` relative-to-cwd works for local dev (`build.sh`/
  `test.sh` always run from `src/`) — not yet verified for however this project eventually gets
  packaged/distributed (an installer, a `.app` bundle, etc.), where "the binary's cwd" may not be
  where a player expects to find/edit a settings file. Not addressed here; revisit once packaging
  is actually decided.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham) — `PlayerOptions.xml`,
  `VLoadGameOptions`, and the book's discussion of writable per-user config living outside both the
  install directory and the packaged resource bundle.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) Decision 2 — `Engine`'s Ch. 5-only scope,
  the boundary this ADR's `EngineConfig`/`GameConfig` split mirrors directly.
- [ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) — `assets/` as read-only,
  version-controlled content; the reasoning this ADR reuses to justify keeping `config/` out of it
  entirely.
- [ADR-0008](0008-data-driven-entity-loading-yaml.md) — `IEntityFileParser`/`EntityDefNode`, reused
  here for the *reading* half; this ADR's YAML-writing need is new, not covered there.
- [`docs/roadmap.md`](../roadmap.md) — tracks this item and its dependency on ADR-0008 landing in
  code.
