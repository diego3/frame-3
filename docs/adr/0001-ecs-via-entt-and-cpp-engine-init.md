# 1. ECS via EnTT, and a C++ `Engine` for application initialization

- Status: Accepted
- Date: 2026-08-02

## Context

frame-3 started as the unmodified [raylib-game-template](https://github.com/raysan5/raylib-game-template)
(plain C, flat `src/`). Two things needed deciding before any real gameplay code could be written:
how game entities are represented and stored, and how the application's own startup/shutdown
lifecycle is structured. Both are addressed here together because the second decision's shape
(whether `Engine` owns an ECS registry) depends on the first.

This repo's build has also gone through several changes today: a layer reorg
(`src/app`/`src/game`/`src/platform`), a Makefile-only consolidation (dropping `CMakeLists.txt`),
and now C++ support added to that same Makefile. The engine-architecture direction below was
originally explored on two side branches (`entt-ecs-dependency`, `engine-architecture-ecs-decision`)
before being ported onto `main` and built on further here.

## Decision 1 — Entities are ECS entities via EnTT, not an Actor class hierarchy

*Game Coding Complete* (Ch. 6-7) spawns game objects as Actors — a class hierarchy, each instance
owning its own fields, typically heap-allocated and pooled. The alternative is an ECS: entities are
just lightweight IDs, components are plain data stored in contiguous arrays the registry owns, and
systems iterate over components directly rather than calling virtual methods on objects.

**Decided: ECS, via [EnTT](https://github.com/skypjack/entt)** (v4.0.0+, requires C++20), not a
hand-rolled ECS and not an Actor hierarchy.

- **Chosen over hand-rolling an ECS** to prioritize reaching actual rendering/gameplay work sooner.
  Hand-rolling a sparse-set ECS is a legitimate, valuable learning exercise in its own right — but
  a *future*, deliberate one, once graphics/gameplay momentum exists, not something to build before
  there's anything to iterate over.
- **Chosen over an Actor hierarchy** because EnTT gets the iteration-cache-friendliness and
  data-oriented layout AAA engines actually use for free, without hand-rolling storage.
- **Not wrapped behind a swappable interface, on purpose.** An ECS isn't a swappable backend
  service the way a `Renderer` interface over GL/Vulkan is — it's the data layout and iteration
  paradigm gameplay code gets written directly against. A future move to a hand-rolled ECS would be
  a deliberate migration regardless of any indirection layer (this is how real engines have made
  this jump — e.g. Unity's GameObject → DOTS rewrote gameplay code, it didn't flip a config flag).
  A "just in case" wrapper here would cost real clarity and performance for a swap unlikely to ever
  be exercised as designed.
- **Consequence for ownership model**: resolved as a side effect, not decided separately. EnTT's
  `entt::registry` owns all component storage itself (sparse sets internally); an entity is a
  lightweight `entt::entity` handle, not a heap-allocated object needing its own
  `unique_ptr`/pool scheme.

### Tradeoffs accepted

- Less pedagogical value than hand-rolling an ECS (deferred, not abandoned — see above).
- A third-party dependency to vendor/track (mitigated: header-only, MIT-licensed, pinned to
  `v4.0.0`, vendored the same way as raylib via `build.sh`/CI).
- Gameplay code is now written directly against EnTT's API vocabulary (`registry.emplace<T>`,
  `registry.view<...>()`) with no insulating layer — accepted per the "not swappable" reasoning
  above; mitigated by writing our own domain-vocabulary helper functions on top as real components
  land (e.g. a future `SpawnEnemy(registry, position)`), not by wrapping the registry itself.

## Decision 2 — `Engine` owns only Ch. 5 Application-layer concerns

*Game Coding Complete* Ch. 5's Application layer (`GameCodeApp` in the book) is responsible for
platform/window/audio lifecycle and driving the main loop — distinct from Ch. 9-10's Game Logic and
Game View layers, which own game state and rendering-per-frame.

**Decided**: `Engine` (`src/app/engine.h`/`.cpp`) owns exactly the Ch. 5 slice: `InitWindow`,
`InitAudioDevice`, loading the resources shared across all screens (font, `fxCoin`), the
`entt::registry`, and driving the main loop (the `PLATFORM_WEB`
`emscripten_set_main_loop`-vs-desktop-`while` branching) via a plain function-pointer callback.
It does **not** touch `screens.h`'s screen state machine (`currentScreen`,
`ChangeToScreen`/`TransitionToScreen`/`UpdateDrawFrame`) — that stays exactly as it was, still
plain C, still fused Update+Draw per screen, called back into by `Engine::Run()` rather than owned
by it.

### Tradeoffs accepted

- A smaller, more honest first abstraction now, vs. a bigger unified App/Logic/View object that
  would have to presuppose how the still-fused screen state machine eventually splits into
  separate Logic and View layers — a question this project has explicitly deferred rather than
  guessed at.
- `Engine` and the screen state machine communicate through a bare function pointer
  (`void (*)(void)`) rather than a richer interface, matching what the code already did (`main()`
  passed `UpdateDrawFrame` to raylib's own `emscripten_set_main_loop` the same way). Revisit if/when
  more than one callback is needed.
- `currentScreen`/`font`/`music`/`fxCoin` remain plain externs defined in `raylib_game.cpp` (not
  `Engine` members), because `screens.h`'s five `screen_*.c` files are still plain C and read them
  via `extern` — moving them into `Engine` would force converting all five screen files to C++ in
  the same change, which is a separate, larger decision not being made here.

## Decision 3 — C++ added to the existing Makefile, not a second build system

The repo consolidated onto a single Makefile-based build earlier today (dropping `CMakeLists.txt`,
one build system to maintain instead of two). EnTT and `Engine` need a C++ toolchain
(`g++`/`clang++`/`em++`, C++20, an EnTT include path) that plain C sources don't.

**Decided**: extend the same Makefile with a parallel `CXX`/`CXXFLAGS`/`%.o: %.cpp` toolchain,
mirroring `CC`/`CFLAGS`'s existing platform-detection logic, rather than reintroducing a second
build system (e.g. CMake, which has first-class multi-language target support) just for the C++
files.

### Tradeoffs accepted

- More Makefile complexity now (two parallel compiler/flag variable sets, one raw text file with no
  language server / IDE understanding of its own logic) vs. CMake's native mixed C/C++ target
  support.
- Consistent with today's earlier, separately-justified decision to consolidate on one build
  system — reopening that tradeoff just for C++ support would have undone it after less than a day.

## Consequences / follow-ups

- `Engine` is the natural home for the not-yet-built systems in
  `.claude/skills/engine-architecture` (event bus, process manager, resource cache) once they
  land — as members alongside `registry_`, not a redesign.
- Windows: `build_windows.yml` and `projects/VS2022/raylib_game/raylib_game.vcxproj` were updated
  to add EnTT and compile the new `.cpp` files as C++20, but **could not be verified locally** (no
  Windows/MSVC available in this environment). Worth a `workflow_dispatch` run to confirm.
- The temporary `src/app/ecs_smoke_test.cpp`/`.h` (used to prove EnTT compiles/links/runs while
  porting the ECS decision onto `main`) has been deleted now that `Engine` gives EnTT a permanent,
  real caller.
