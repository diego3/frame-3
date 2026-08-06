# 14. Game modules own game code; `app/` remains a game-agnostic engine

- Status: Accepted
- Date: 2026-08-05

## Context

frame-3 began from raylib's advanced-game template. Most of its application flow remains in C:
`screen_*.c`, `screens.h`, and `app/raylib_game.cpp` form one global screen state machine using
global `Font`, `Music`, and `Sound` values. That code is useful bootstrap code, but it is not the
architecture intended for the project: it couples platform startup, asset choices, screen flow,
and game behavior in one executable.

The engine systems added since then already have a different boundary. `Engine`, events,
processes, resources, transforms, entity definitions, and level loading are intended to be reused
by more than one game. `GameConfig` is also explicitly owned by a game rather than `Engine`
(ADR-0011). The template code currently violates that direction: `app/engine.cpp` includes
`../game/screens.h` and loads the template font and coin sound itself.

The local *Game Coding Complete* reference follows the same high-level split. `Source/GCC4/`
contains generic framework code, while `Source/TeapotWars/` contains `TeapotWarsApp`,
`TeapotWarsLogic`, its events, controller, and views. The TeapotWars project links the GCC4
library; GCC4 does not include TeapotWars headers. The book's Windows/DirectX implementation and
its inheritance-heavy application class are not copied literally, but that dependency direction is
the relevant principle.

ADR-0010 proposes the first real Logic/View split. Before implementing it, the project needs to
state where generic contracts end and where the first concrete game begins; otherwise that work
could reintroduce template-specific knowledge into `Engine`.

## Decision

### `src/app/` is a game-agnostic engine

`src/app/` contains reusable C++ engine code only: platform lifecycle, main-loop mechanics,
resource-cache primitives, ECS/world services, events, processes, loading, and generic interfaces.
It may depend on raylib and vendored libraries, but it must not include, construct, or name a
specific game's types, assets, entity definitions, rules, screen flow, input bindings, or UI.

Generic extension contracts belong here when they are useful independently of one game. The
`BaseGameLogic`/`IGameView` interfaces proposed by ADR-0010 are such contracts. A concrete
`HumanView`, player controller, render component convention, HUD, or menu remains game-layer code
until a second game creates an evidenced need to promote a genuinely generic part.

`Engine` continues to own the window/audio lifecycle and generic per-frame work. It accepts a
game-owned callback or generic interface, but does not select screens, load game assets, or retain
game globals. Template font/music/sound ownership therefore moves out of `Engine` as the template
is migrated.

### Each game is a separately composable module under `src/game/`

Every concrete game lives below `src/game/<game-id>/`. It owns its composition root, game config,
assets it chooses to load, entity/level selection, gameplay rules, concrete views, controllers,
and UI/screen flow. Its composition root constructs `Engine`, supplies the title and `EngineConfig`,
and wires game code to engine contracts. Dependencies flow only from a game module to `app/`, never
from `app/` back to a game module.

The current raylib-template game is a transitional first module. Its existing flat
`src/game/screen_*.c`/`screens.h` files are migrated into one named game-module directory as their
ownership is clarified; new game-specific source must be C++ (`.cpp`/`.h`). No new C screen files
or new cross-module globals are added. During migration, a small `extern "C"` bridge is allowed
only where an unchanged template callback requires it, and is removed with the corresponding
template screen.

### Build selection is explicit and static

The Makefile builds one selected game together with the engine. It will gain a game selection
variable and each game module contributes its own source list; this is a build-time choice, not a
runtime plugin system. A static composition keeps the current Makefile-based project simple while
allowing a second game without changing engine source or coupling games to each other.

The executable entry point moves from `src/app/raylib_game.cpp` into the selected game module. The
engine exposes no `main()` and no template-screen symbols. Shared platform packaging may remain in
`src/platform/`.

## Consequences and sequencing

- The first implementation change creates the named module directory, moves the entry point and
  template screens into it, and changes the Makefile source lists without changing gameplay
  behavior. It also removes `screens.h` from `Engine` and makes the game own its resources.
- ADR-0010 is implemented after or alongside that extraction: `BaseGameLogic` and `IGameView`
  enter `app/`; the first concrete view and game-loop bridge enter the selected game module.
- Tests for reusable behavior stay in `src/tests/`; tests for game rules live with or are clearly
  named for their game module. Engine tests must never include game headers.
- A game may temporarily retain the template's logo/title/options/ending flow. Replacing that flow
  with a UI system is separate work and does not justify putting it into `Engine` prematurely.
- This decision does not introduce dynamic loading, a game registry, a plugin ABI, or a generic
  menu/UI framework. Revisit those only when more than one built game demonstrates a need.

## Tradeoffs accepted

- Moving files and removing globals creates short-term churn before visible gameplay improves.
  The payoff is that the first game cannot silently become part of the engine API.
- The selected-game Makefile option is less convenient than runtime selection, but avoids a plugin
  architecture with no concrete consumer.
- Some code initially placed in a game module may later prove reusable. Promotion into `app/` is
  deliberate and evidence-driven, rather than assuming the first game's needs are engine needs.

## References

- *Game Coding Complete*, 4th edition: `Source/GCC4/` and `Source/TeapotWars/` in the local
  `/home/diego/Documents/gamecode4` checkout.
- ADR-0001 — `Engine` is scoped to application-layer concerns.
- ADR-0010 — proposed `BaseGameLogic`/`IGameView` split.
- ADR-0011 — `GameConfig` is game-owned, not engine-owned.
