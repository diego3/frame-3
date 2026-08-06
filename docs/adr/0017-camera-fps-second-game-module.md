# 17. Second game module (`game/camera_fps`), `GAME` build selector, `HumanViewBase` promoted to `app/`

- Status: Accepted
- Date: 2026-08-05

## Context

Every ADR through 0016 was designed and implemented against a single concrete game module,
`game/sandbox/` — everything `app/` exposes has one real caller. [ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md)
named exactly this as the reason it deferred any generalization of `app/`: "an API frozen from a
single consumer's usage is a guess, not a validated boundary — the second consumer is what tells
you where the abstraction actually leaks." That ADR also specifically flagged a generic
`HumanView` base as "deferred, gated on a second game/consumer" — repeated again in
[ADR-0016](0016-screen-element-stack.md)'s roadmap note.

This ADR records that second consumer actually landing: raylib's own
`examples/core/core_3d_camera_fps.c` ("raylib [core] example - 3d camera fps" — WASD+mouse-look
FPS body movement over a small hardcoded level), ported in as `src/game/camera_fps/`, alongside
`game/sandbox/` rather than replacing it. Chosen over a minimal standalone-raylib port specifically
so it would exercise the `IGameView`/`IScreenElement` seam as a second real consumer, not duck the
question.

## Decision

### The example doesn't need `BaseGameLogic`, `EntityFactory`/`LevelLoader`, or `GameConfig`

Its "level" (floor tiles, 4 towers, a red sun) is entirely procedural/hardcoded geometry in one
`DrawLevel()` function — not entity data. It has no ECS actor at all: the "player" is
camera-attached state, never itself drawn or possessed via a `LocalTransform`. And it uses no
textures or sounds. Every one of those `app/` pieces stays exactly as game-agnostic as it already
was; this module simply doesn't call into three of them, the same way `game/sandbox/` doesn't call
into `IGamePhysics` (unbuilt) or a network transport (unbuilt). Forcing a YAML level file for 4
fixed towers, or a `GameConfig` for zero assets, would have added ceremony without reusing
anything real.

### What *does* generalize: `HumanViewBase`, promoted out of `game/sandbox/human_view.*` into `app/`

`game/sandbox/human_view.h`/`.cpp`'s `HumanView` mixed two things: generic `IScreenElement` stack
plumbing (`PushElement`/`RemoveElement`, the sorted `VOnRender` dispatch, `VOnAttach` storing
`id_`/`possessedActor_`) and sandbox-specific control logic (arrow-key box movement reading a
`LocalTransform`). `game/camera_fps/human_view.h`'s `CameraFpsView` needs exactly the first half —
same stack mechanics — and a totally different second half (no ECS actor, no
`ProcessManager`/`ResourceCache<Sound>` dependency, mouse+WASD FPS movement instead of arrow-key
box nudging). That's precisely the second-data-point signal ADR-0015 said to wait for, so:

- **New**: `app/human_view_base.h`/`.cpp` — abstract `HumanViewBase : public IGameView`. Implements
  `VOnAttach`, `VOnRender` (stable-sort by z-order, dispatch to visible elements — unchanged from
  the body `HumanView::VOnRender` had before), `VGetType()`, `PushElement`/`RemoveElement`, and a
  protected `UpdateElements(dt)` helper a subclass's own `VOnUpdate` calls explicitly (so it
  controls ordering against its own input handling — matches what `HumanView::VOnUpdate` already
  did). Leaves `VOnUpdate` itself pure virtual. `possessedActor_` is a protected member — sandbox's
  subclass uses it, camera_fps's subclass never sets it.
- **`game/sandbox/human_view.h`/`.cpp`**: `HumanView` now `: public HumanViewBase`, keeping only
  `VOnUpdate` and its own `registry_`/`processes_`/`sounds_`/`camera_` members. Pure refactor, no
  behavior change — same `GameplayScene`/`GameplayHud` elements, same movement math, verified via
  the full test suite and a headless smoke test.
- **`game/camera_fps/human_view.h`/`.cpp`**: `CameraFpsView : public HumanViewBase`, with the
  example's `Body` struct (as a free `CameraFpsBody`, so the `FpsHud` element can read it),
  movement constants, and `UpdateBody`/`UpdateCameraFPS` math ported field-for-field into
  `VOnUpdate`/private helpers. Two `IScreenElement`s pushed in the constructor, mirroring sandbox's
  split: `FpsScene` (the 3D pass — `BeginMode3D`/`DrawLevel()`/`EndMode3D`) and `FpsHud` (the
  "Camera controls:" info box + live velocity readout).
- **`game/camera_fps/main.cpp`**: no `screens.h`-style multi-screen state machine — the original
  example has none either, and sandbox's Logo/Title/Options/Ending machinery is sandbox content,
  not part of the pattern being reused. `Engine::Init`/`DisableCursor`/construct the view/call
  `VOnAttach(1, std::nullopt)` directly (no `BaseGameLogic` to do it)/`engine.Run(...)` over a
  small `UpdateDrawFrame`. Same `UpdateDebugOverlay`/`DrawDebugOverlay` (F3 HUD) wiring sandbox's
  `main.cpp` has — that's app-global per ADR-0016, not sandbox-specific.

This is deliberately **not** the rest of what ADR-0015 described (a versioned public API,
compatibility guarantees, packaging/distribution, external-consumer docs). Nothing here requires
any of that — the actual shared surface two real consumers needed was small and mechanical
(`HumanViewBase`), not a hint that the whole `app/` boundary needs freezing. See ADR-0015's own
follow-up note.

### `src/Makefile`: a `GAME` build selector

First time two game modules coexist in-tree, so first time the Makefile needs to choose one:
`GAME ?= sandbox`, gating each module's file list behind `ifeq ($(GAME),...)`, with the
game-agnostic `app/*.cpp` files (including the new `human_view_base.cpp`) staying unconditional.
`PROJECT_NAME`/the output binary name (`raylib_game`) is unchanged regardless of `GAME` — switching
`GAME` just changes what that one binary contains, same as switching branches already did before
this existed, so `run.sh`/`.gitignore`/`README.md`'s existing assumptions about that path stay
valid. `build.sh` passes `GAME=${GAME:-sandbox}` through to `make`.

### CI

`.github/workflows/ci_sanity.yml` gained one extra build-only step, `GAME=camera_fps`, right after
the existing default (`sandbox`) build — proves both modules actually compile clean. The four
platform release workflows (`build_linux/macos/windows/webassembly.yml`) are unchanged — they build
the shipped product (`sandbox`), not every module in-tree.

## Tradeoffs

- **`FpsScene`/`FpsHud` duplicate the shape of `GameplayScene`/`GameplayHud`, not their code.**
  Both pairs are small (a handful of lines each) and render completely different content — no
  further extraction was warranted from two data points; revisit only if a third game's element
  looks the same again.
- **`camera_fps` has no way to quit back to a menu or exit gracefully** (same as the original
  raylib example — window-close/Esc only). Not a regression against anything this module promised;
  just worth noting it doesn't demonstrate `screens.h`-style transitions, since it deliberately
  doesn't use them.
- **The `GAME` Makefile variable is untyped** — `GAME=nonsense` silently produces an
  engine-only binary with `main()` missing (link failure), not a friendly error. Acceptable for a
  two-option internal dev switch; revisit if this grows past a handful of modules.

## Consequences

- [ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md) gets a short "trigger met"
  follow-up note pointing back here — its own Status stays `Proposed`, since the broader
  productization decision it describes still hasn't happened.
- [`docs/roadmap.md`](../roadmap.md)'s ADR-0016 entry is updated: the "still deferred, gated on a
  second game/consumer" clause is now resolved, linking here.
- `.claude/skills/engine-architecture/SKILL.md` §10 updated to mention `HumanViewBase` (`app/`) and
  `game/camera_fps` as the second concrete `IGameView` consumer.

## What actually shipped, alongside this ADR

Implemented in the same change: `app/human_view_base.h`/`.cpp` (new), `game/sandbox/human_view.h`/
`.cpp` (refactored to subclass it, no behavior change), `game/camera_fps/human_view.h`/`.cpp` and
`game/camera_fps/main.cpp` (new), the `GAME` Makefile variable and `build.sh` passthrough, and the
`ci_sanity.yml` second build step. Verified via: `make ... GAME=sandbox` and `make ...
GAME=camera_fps` both building clean under `-Werror`; the full unit test suite (83/83) passing
unmodified; and a headless (`xvfb-run`) smoke test of both binaries running several hundred frames
each with no crash (sandbox's GAMEPLAY screen reached via a temporary, fully-reverted `main.cpp`
patch, since no `xdotool` is available in this environment to drive screen transitions).

## References

- [ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md) — the deferred decision
  this ADR's `HumanViewBase` promotion resolves the narrow part of.
- [ADR-0016](0016-screen-element-stack.md) — the `IScreenElement` stack this reuses unchanged.
- [ADR-0010](0010-base-game-logic-and-igameview.md) — `IGameView`/`HumanView`'s original design.
- [ADR-0014](0014-game-module-boundary-and-template-migration.md) — the `app/`/`game/<game-id>/`
  boundary this module is the second occupant of.
- `/home/diego/Documents/raylib/examples/core/core_3d_camera_fps.c` — the ported example (raylib
  6.0-era checkout; distinct from the older, simpler `core_3d_camera_first_person.c`).
