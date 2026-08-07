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

### The player and its 4 towers are real actors, loaded like `game/sandbox`'s own level

First pass at this module kept the example's own shape almost entirely intact: the "player" stayed
camera-attached view state (no ECS actor), and the 4 towers stayed hardcoded draw calls inside one
`DrawLevel()` function, on the reasoning that none of it was genuinely data. On review, that
reasoning didn't hold up: the towers are placed objects with a position/size/color exactly like any
other entity this project spawns, and the player not being a real actor meant `possessedActor_`
(`HumanViewBase`'s whole reason to exist) went unused by the one view that had just motivated
promoting it. Revised to actually exercise the engine's data-driven path a second time, not just
its `IScreenElement` seam:

- **`assets/levels/camera_fps.yaml`** (new): the player (reusing `assets/entities/player.yaml`
  as-is — schema-identical to what it already was) plus 4 `assets/entities/tower.yaml` actors, each
  overriding `position` to one of the example's original 4 corners. Loaded through
  `BaseGameLogic`/`EntityFactory`/`LevelLoader`, the same wiring `game/sandbox/screen_gameplay.cpp`
  already uses — `game/camera_fps/main.cpp` registers `"Position"` (identical to sandbox's own
  loader) and a new `"BoxRenderable"` loader.
- **`app/render_components.h`** (new): `BoxRenderable { Vector3 size; Color color; }` — the first
  real "how does an entity look" component (ADR-0010's own Open Questions flagged this as
  undecided). Game-agnostic by nature, so it lives in `app/`, not under `game/camera_fps/` — but
  not applied to `game/sandbox`'s own `GameplayScene` (still hardcodes 1×1×1 MAROON wireframes for
  every entity) in this change; revisit that only if sandbox itself needs entity-driven appearance.
- **`game/camera_fps/components.h`** (new): `PlayerBody { Vector3 velocity, dir; bool isGrounded; }`
  — the example's file-local `Body` struct, minus `position` (that's the entity's own
  `LocalTransform`/`WorldTransform`, not duplicated). Kept game-local, not promoted to `app/` like
  `BoxRenderable` — its fields and the algorithm driving them (gravity, friction, air drag, the
  acceleration curve) are tuned specifically to this FPS movement scheme, not a generic "physics
  body" `app/` has any other consumer for. That's `ADR-0012`'s (still-`Proposed`) job to design
  once a second game actually needs its own movement scheme too, not something to guess ahead of
  from one data point.
- **`app/scene_renderer.h`** (new): `DrawBoxRenderables(entt::registry&)` — a generic, header-only
  free function that draws every `BoxRenderable` entity at its already-computed `WorldTransform`
  (ADR-0002's scene graph). Pulled out of `FpsScene::VOnRender`'s own hand-rolled loop specifically
  so the scene-graph-driven part of rendering isn't duplicated per view — any `HumanView`-family
  view can call it from inside its own `BeginMode3D`/`EndMode3D` block (which `Camera3D` is active,
  and how a view stores one, stays the caller's problem — `game/sandbox` keeps a plain member,
  `game/camera_fps` keeps one on `FirstPersonCameraRig`, and this function doesn't need to care).
  `game/sandbox`'s `GameplayScene` doesn't call it yet (its player entity has no `BoxRenderable`),
  but nothing would stop it from adopting the exact same call once it does.
- The floor tiles and the sun stay procedural, drawn directly in `FpsScene::VOnRender` — scene
  dressing, not placed objects, the same category `game/sandbox`'s own `DrawGrid()` call already
  sits in outside the ECS.
- **`BaseGameLogic::VLoadLevel`'s return type** changed from `void` to `std::vector<entt::entity>`
  (forwarding `LevelLoader::Load`'s own return value, previously discarded). Needed here because
  `game/camera_fps.yaml` is the first level in this project with more than one entity, so "the
  first entity in registry iteration order is the player" (what `game/sandbox` used until now)
  stops being a safe assumption — the level file's own `actors[]` order is unambiguous regardless
  of registry/storage iteration order, so callers now take `spawned[0]` directly.
  `game/sandbox/screen_gameplay.cpp` was updated to the same pattern for consistency (its own
  comment already flagged this as "revisit once a level has more than one entity" — this is that).

### What *does* generalize: `HumanViewBase`, promoted out of `game/sandbox/human_view.*` into `app/`

`game/sandbox/human_view.h`/`.cpp`'s `HumanView` mixed two things: generic `IScreenElement` stack
plumbing (`PushElement`/`RemoveElement`, the sorted `VOnRender` dispatch, `VOnAttach` storing
`id_`/`possessedActor_`) and sandbox-specific control logic (arrow-key box movement reading a
`LocalTransform`). `game/camera_fps/human_view.h`'s `CameraFpsView` needs exactly the first half —
same stack mechanics — and a totally different second half (mouse+WASD FPS movement instead of
arrow-key box nudging, driving a different set of components). That's precisely the
second-data-point signal ADR-0015 said to wait for, so:

- **New**: `app/human_view_base.h`/`.cpp` — abstract `HumanViewBase : public IGameView`. Implements
  `VOnAttach`, `VOnRender` (stable-sort by z-order, dispatch to visible elements — unchanged from
  the body `HumanView::VOnRender` had before), `VGetType()`, `PushElement`/`RemoveElement`, and a
  protected `UpdateElements(dt)` helper a subclass's own `VOnUpdate` calls explicitly (so it
  controls ordering against its own input handling — matches what `HumanView::VOnUpdate` already
  did). Leaves `VOnUpdate` itself pure virtual. `possessedActor_` is a protected member both
  subclasses now actually use.
- **`game/sandbox/human_view.h`/`.cpp`**: `HumanView` now `: public HumanViewBase`, keeping only
  `VOnUpdate` and its own `registry_`/`processes_`/`sounds_`/`camera_` members. Pure refactor, no
  behavior change — same `GameplayScene`/`GameplayHud` elements, same movement math, verified via
  the full test suite and a headless smoke test.
- **`game/camera_fps/human_view.h`/`.cpp`**: `CameraFpsView : public HumanViewBase` holds *no*
  per-frame state of its own — not even the `Camera3D`. Everything (`Camera3D`, look-rotation,
  head-bob easing) lives on the possessed actor as a new `FirstPersonCameraRig` component
  (`game/camera_fps/components.h`), seeded by a `VOnAttach` override the moment a view actually
  possesses an actor (view/presentation setup — kept separate from `main.cpp`'s `PlayerBody`
  emplace, which is simulation state the game logic owns). `UpdateBody`/`UpdateCameraFPS` became
  free functions taking components by reference instead of view methods — they no longer need
  `this` for anything. Three `IScreenElement`s pushed in the constructor: `FpsScene` (the 3D pass —
  floor/sun procedural, towers drawn via `app/scene_renderer.h`'s `DrawBoxRenderables`, camera
  re-fetched from `FirstPersonCameraRig` every call — see below), `FpsHud` (the "Camera controls:"
  info box + live velocity readout, reading `PlayerBody.velocity`), and `DebugOverlayScreenElement`
  (next bullet).

  **Nothing may cache a pointer into an ECS component across frames** — entt can relocate a
  component pool's backing storage on any create/destroy of that same component type. `FpsScene`
  used to hold a `const Camera3D&` captured once at construction (safe, back when `Camera3D` was a
  stable view member); now that it's component data, `FpsScene`/`CameraFpsView` re-fetch
  `FirstPersonCameraRig` via `registry.try_get<...>(actor)` inside every call instead. Verified via
  the same headless smoke test (several hundred frames, no crash), not just asserted in a comment.
- **`app/debug_overlay_screen_element.h`** (new): `DebugOverlayScreenElement`, a header-only
  `IScreenElement` wrapping `DebugOverlay` (F3 HUD) — promoted to `app/` rather than left as a
  `camera_fps`-local class, since wrapping an already-generic system is itself generic; any future
  single-view game can reuse it as-is. [ADR-0016](0016-screen-element-stack.md) still deliberately
  keeps `DebugOverlay` *outside* `game/sandbox`'s own `HumanView` stack, because sandbox's overlay
  must survive `LOGO`/`TITLE`/`OPTIONS`/`ENDING` — screens where no `HumanView` exists yet — so
  folding it in there would regress it. That reasoning doesn't transfer to `camera_fps`: there's
  exactly one view, alive for the game's entire run, so there's no screen where folding it in would
  lose coverage. `game/camera_fps/main.cpp`'s `UpdateDrawFrame` doesn't reference `DebugOverlay` at
  all anymore — `game/sandbox/main.cpp` still calls `UpdateDebugOverlay`/`DrawDebugOverlay`
  directly, and ADR-0016's reasoning for that stays correct and unchanged there. Not a reversal of
  that ADR, a narrower case (and now a reusable header) it didn't need to cover.
- **`game/camera_fps/main.cpp`**: no `screens.h`-style multi-screen state machine — the original
  example has none either, and sandbox's Logo/Title/Options/Ending machinery is sandbox content,
  not part of the pattern being reused. `Engine::Init`/`DisableCursor`, then the same
  `EntityFactory`/`LevelLoader`/`BaseGameLogic` wiring `screen_gameplay.cpp` uses to load
  `camera_fps.yaml` and resolve the player actor, construct the view, `AttachView`, and
  `engine.Run(...)` over a small `UpdateDrawFrame` that just ticks `BaseGameLogic::VOnUpdate`
  (which ticks the view) and renders — no other per-system code left in it at all.

This is deliberately **not** the rest of what ADR-0015 described (a versioned public API,
compatibility guarantees, packaging/distribution, external-consumer docs). Nothing here requires
any of that — the actual shared surface two real consumers needed was small and mechanical
(`HumanViewBase`), not a hint that the whole `app/` boundary needs freezing. See ADR-0015's own
follow-up note.

### `CameraFpsLogic`: the movement simulation moves into a real `BaseGameLogic` subclass

Earlier revisions of this module still ran `UpdateBody` (the FPS movement/physics integration)
directly from `CameraFpsView::VOnUpdate`, reading raw input and writing `PlayerBody`/
`LocalTransform` in the same method. That's exactly the coupling
[ADR-0010](0010-base-game-logic-and-igameview.md)'s Logic/View split exists to prevent — its own
`VLoadLevel` doc comment already left "a future game-specific `BaseGameLogic` subclass" as an
explicit seam, unused until now because `game/sandbox` never needed anything beyond loading a
level. `game/camera_fps` does:

- **New**: `game/camera_fps/game_logic.h`/`.cpp` — `CameraFpsLogic : public BaseGameLogic`,
  overriding the now-`virtual` `BaseGameLogic::VOnUpdate` (was non-virtual; this is the first
  subclass to need overriding it, the same reason `VLoadLevel` was already `virtual`). Each tick:
  advances every actor with a `MovementIntent`/`PlayerBody`/`LocalTransform` one physics step (the
  `UpdateBody` math, moved here verbatim from `human_view.cpp`, along with its physics-only
  constants — gravity, friction, air drag, the acceleration curve), *then* calls
  `BaseGameLogic::VOnUpdate` to tick attached views. `registry_.view<...>()` here *is* this
  project's ECS-native form of "`GameLogic` walks its actors, updating their components" — there's
  no separate actor list to iterate by hand the way an OOP actor-with-components model would need;
  the view itself is that walk, scoped to exactly the actors with every component this one system
  cares about.
- **New component, deliberately not player- or camera-specific**: `MovementIntent`
  (`game/camera_fps/components.h`) — `facingYaw`/`side`/`forward`/`jumpPressed`/`crouchHold`. The
  seam that crosses the Logic/View boundary now that physics moved out of the view:
  `PlayerMovementElement::VOnUpdate` (next section) still reads raw mouse/keyboard every frame
  (legitimately View's job — turning hardware input into meaning), but now just writes the result
  as `MovementIntent` instead of computing physics from it directly. Named and shaped so nothing
  about it is human-specific: `CameraFpsLogic`'s physics step only ever reads whatever
  `MovementIntent` an actor has — it has no idea, and doesn't need one, whether a human or a future
  `AIView`'s decision tree produced it. Swapping the player's controller for an AI later means
  writing an `AIView` that emplaces this same component with its own decided values;
  `CameraFpsLogic` doesn't change. `facingYaw` (not `lookYaw`) is the same reasoning applied to the
  field itself — it's "which way this actor is facing, for resolving movement direction," a fact
  about the actor, not about a rendering camera, even though its only current source happens to be
  `FirstPersonCameraRig.lookRotation.x` (still deliberately copied rather than read directly, for
  the same reason).
- **Ordering, and the tradeoff it accepts**: `CameraFpsLogic::VOnUpdate` runs the physics step
  *before* ticking views, so `CameraFpsView`'s camera-position update reads this frame's
  already-integrated `LocalTransform` (no lag there). The cost: the physics step consumes
  `MovementIntent` as it stood after the *previous* frame's view tick, since this frame's view tick
  (which would overwrite it) hasn't run yet — a one-frame input-to-physics lag. Standard for any
  engine with this split, imperceptible at real frame rates. Self-healing on boot, too: frame 0's
  physics step finds no actor with a `MovementIntent` yet (no view has ticked) and simply skips it,
  not a special case to handle.
- **`EventManager` use, via the seam `BaseGameLogic` already exposes**: the ported example's own
  `UpdateBody()` had a commented-out "Sound can be played at this moment" hook right where a jump
  triggers. `CameraFpsLogic::VOnUpdate` now queues a new `EvtData_ActorJumped{entity}`
  (`game_logic.h`) via `events_` at exactly that point — the same "fire it even with no subscriber
  yet" precedent `app/level_loader.h`'s `EvtData_EntitySpawned` already set (ADR-0009). `UpdateBody`
  itself stays a pure function of its inputs/outputs (returns whether it triggered a jump, no
  `EventManager` dependency of its own) — `CameraFpsLogic::VOnUpdate` queues based on the result.

### `PlayerMovementElement`: the per-frame input/camera work becomes its own `IScreenElement`

Even after physics moved to `CameraFpsLogic`, `CameraFpsView::VOnUpdate` still directly held ~40
lines of algorithm: reading raw input, publishing `MovementIntent`, and easing the camera
(`FirstPersonCameraRig`'s head-bob/FOV/lean state) to follow the possessed actor. That's exactly
the same shape `FpsScene`/`FpsHud`/`DebugOverlayScreenElement` already existed to avoid for
rendering — nothing said per-frame *update* work couldn't get the same treatment:

- **New**: `game/camera_fps/human_view.cpp`'s `PlayerMovementElement : IScreenElement` — owns what
  `VOnUpdate` used to: reads mouse/keyboard, writes `MovementIntent`, calls a newly-factored-out
  `UpdateHeadBobEasing` (mirrors `UpdateCameraFps` already being its own function) and
  `UpdateCameraFps`. `VOnRender` is a no-op — nothing here draws.
- **`CameraFpsView` stops overriding `VOnUpdate` entirely.** Its whole per-frame job was always
  just `UpdateElements(dt)` once `PlayerMovementElement` existed to do the real work, so
  `app/human_view_base.h`'s `HumanViewBase::VOnUpdate` gained a default body (`UpdateElements(dt)`)
  — non-`virtual`-`=0` now, specifically so a subclass whose entire "human" update *is* element
  dispatch doesn't need to override it at all. `game/sandbox`'s `HumanView` still overrides it
  (it also moves its possessed actor directly, not through a pushed element) — unaffected.
  `CameraFpsView` itself is left holding only composition (its constructor) and `VOnAttach` (an
  `IGameView` lifecycle hook with no `IScreenElement` equivalent to delegate to).

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
  Both pairs are small and render different content — no further extraction was warranted from two
  data points; revisit only if a third game's element looks the same again.
- **The floor tiles and sun are still hardcoded**, not entities — deliberately (see Decision): they
  aren't placed objects, they're a backdrop, same category as `game/sandbox`'s `DrawGrid()`. Worth
  re-examining only if a future game needs a non-trivial *procedural* floor to also be data-driven,
  which neither game does today.
- **`CameraFpsLogic`'s physics step is still the same hand-written `UpdateBody` function**, not
  anything resembling `IGamePhysics` — it lives in the right *layer* now (Logic, not View), but
  it's still one game's bespoke movement math, not a reusable simulation system. That's still
  ADR-0012's (`Proposed`) job; this ADR only fixes *where* camera_fps's own physics lives, not what
  a real physics abstraction would look like.
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
  `game/camera_fps` as the second concrete `IGameView` consumer, and its own components/level.
- `BaseGameLogic::VLoadLevel`'s new return type is a small, backward-compatible signature change
  (existing callers ignoring the return value still compile) that also fixed a real latent
  ambiguity in `game/sandbox/screen_gameplay.cpp` once this ADR's level file exposed it — see
  Decision above.

## What actually shipped, alongside this ADR

Implemented in the same change: `app/human_view_base.h`/`.cpp` (new, plus a later default
`VOnUpdate` body), `app/render_components.h`, `app/scene_renderer.h`,
`app/debug_overlay_screen_element.h` (all new); `app/base_game_logic.h`/`.cpp`'s `VLoadLevel`
return-type change and `VOnUpdate` becoming `virtual`, plus new test coverage; `game/sandbox/
human_view.h`/`.cpp` (refactored onto `HumanViewBase`) and `screen_gameplay.cpp` (updated to
`VLoadLevel`'s returned entity list) — both pure refactors, no behavior change; `game/camera_fps/
human_view.h`/`.cpp` (`PlayerMovementElement`, `FpsScene`, `FpsHud`), `main.cpp`, `components.h`
(`PlayerBody`, `FirstPersonCameraRig`, `MovementIntent`), `game_logic.h`/`.cpp` (`CameraFpsLogic`,
`EvtData_ActorJumped`, new); `assets/levels/camera_fps.yaml`, `assets/entities/tower.yaml` (new);
the `GAME` Makefile variable and `build.sh` passthrough; the `ci_sanity.yml` second build step.
Verified via: `make ... GAME=sandbox` and `make ... GAME=camera_fps` both building clean under
`-Werror`; the full unit test suite (84/84, including the new `VLoadLevel` coverage) passing; and a
headless (`xvfb-run`) smoke test of both binaries running several hundred frames each with no crash
(sandbox's GAMEPLAY screen reached via a temporary, fully-reverted `main.cpp` patch, since no
`xdotool` is available in this environment to drive screen transitions) — the `camera_fps` run in
particular exercises the "never cache a pointer into a component" rule above under real per-frame
churn, not just in theory.

## References

- [ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md) — the deferred decision
  this ADR's `HumanViewBase` promotion resolves the narrow part of.
- [ADR-0016](0016-screen-element-stack.md) — the `IScreenElement` stack this reuses unchanged.
- [ADR-0010](0010-base-game-logic-and-igameview.md) — `IGameView`/`HumanView`'s original design.
- [ADR-0014](0014-game-module-boundary-and-template-migration.md) — the `app/`/`game/<game-id>/`
  boundary this module is the second occupant of.
- `/home/diego/Documents/raylib/examples/core/core_3d_camera_fps.c` — the ported example (raylib
  6.0-era checkout; distinct from the older, simpler `core_3d_camera_first_person.c`).
