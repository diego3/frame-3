# 10. `BaseGameLogic` + `IGameView`: activating the Logic/View split

- Status: Proposed
- Date: 2026-08-04

## Context

[ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) Decision 2 deliberately kept `Engine` scoped
to *Game Coding Complete* Ch. 5's Application layer only, explicitly deferring whether/how to
split Ch. 9-10's Game Logic from Game View — "a question this project has explicitly deferred
rather than guessed at." [ADR-0009](0009-level-loading-actor-placement.md) revisited that question
directly and answered **not yet**: the actor map (`entt::registry`), process management
(`ProcessManager`), and level loading (`LevelLoader`) — three of the pieces `BaseGameLogic` bundles
in the book — already exist here as separate systems, so nothing about loading a level *needed* a
`BaseGameLogic` host object. ADR-0009 named two concrete triggers for revisiting: `screens.h`'s
fused Update+Draw model straining under its own growth, or a multiplayer server loop needing
simulation with no drawing at all (per [ADR-0005](0005-event-manager-queued-dispatch-idata-lua-proposal.md)'s
networking proposal) — and left the question open rather than guessing ahead of either.

This ADR is prompted by a direct decision to build the split now, ahead of either trigger being
literally hit: `HumanView`, `RemoteView`, `AIView`, and game-specific `BaseGameLogic` subclasses
are the explicitly next-planned work, and every one of them needs `IGameView`/`BaseGameLogic` to
exist first. Retrofitting the seam after any of those get built independently (e.g. a `HumanView`
built as a one-off, then having to be reshaped into `IGameView`'s contract once `RemoteView` shows
up) costs more than building the seam once, now, while only one concrete view exists to prove it
against. This is the same category of call ADR-0009 already made once, explicitly, for firing
`EvtData_EntitySpawned` with no subscriber yet: a deliberate exception to this project's usual
"don't build ahead of need" discipline, made because *this specific seam* is the one place where
waiting for a fuller trigger means touching every dependent view/logic class again the day it
actually arrives.

**What "the book's flow" means here, concretely**: `BaseGameLogic` owns a list of attached
`IGameView`s and never knows or cares how many there are, what kind, or how they render/replicate/
decide — it just updates simulation state and fires events. Views pull what they need from shared
state (the registry, for a same-process renderer) or react to events pushed at them (for anything
that can't pull, like a network client). That decoupling — not any particular method signature —
is the part being adopted; the rest of this ADR adapts the concrete shape to raylib/EnTT and drops
book mechanics (Win32 message procs, DirectX device-loss recovery) with no analogue here.

## Decision

### 1. `IGameView` — adapted interface

```cpp
// game_view.h (sketch)
enum class GameViewType { Human, Remote, AI, Other };
using GameViewId = std::uint32_t;

class IGameView {
public:
    virtual ~IGameView() = default;

    // Called once, when BaseGameLogic::AttachView adds this view. actorId is the entity this view
    // is bound to controlling/following, if any (e.g. a HumanView's player-controlled entity) --
    // std::optional because a RemoteView or an observer-only view may not possess an actor at all.
    virtual void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) = 0;

    // Input + view-local state (camera, UI, network send/recv, AI decision-making -- whatever this
    // concrete view is for). Never touches rendering.
    virtual void VOnUpdate(float dt) = 0;

    // Rendering only, for views that do any (a RemoteView's VOnRender is typically a no-op).
    virtual void VOnRender(float dt) = 0;

    virtual GameViewType VGetType() const = 0;
    GameViewId GetId() const { return id_; }

protected:
    GameViewId id_ = 0;   // set by VOnAttach
};
```

Dropped from the book's `IGameView`, with no raylib/OpenGL analogue at this project's scope:

- **`VOnRestore()`/device-loss recovery** — a DirectX concept (device lost on alt-tab/resolution
  change, resources need re-creating). raylib/OpenGL doesn't expose an equivalent hook to recover
  from; nothing here needs it until a real platform surfaces a real problem.
- **`VOnMsgProc(AppMsg)`** — Win32 message-loop translation (raw window messages → engine input
  events). raylib already hands out polled input state directly (`IsKeyDown`, `GetGesture...`,
  `GetMouseDelta`, ...); `HumanView::VOnUpdate` reads those directly, no translation layer needed.
- **Two-arg `VOnRender(double gameTime, float elapsedTime)`** — collapsed to a single `dt`, since
  nothing built so far has needed absolute elapsed game time separately from the frame delta;
  revisit only if a concrete need for it shows up (e.g. a replay/recording view).

### 2. `BaseGameLogic` — adapted

```cpp
// base_game_logic.h (sketch)
enum class GameLogicState { Loading, Running, Paused };

class BaseGameLogic {
public:
    BaseGameLogic(entt::registry &registry, EventManager &events, ProcessManager &processes,
                  LevelLoader &levelLoader);
    virtual ~BaseGameLogic() = default;

    GameViewId AttachView(std::unique_ptr<IGameView> view,
                          std::optional<entt::entity> actorId = std::nullopt);
    void DetachView(GameViewId id);

    // Loads levelPath via levelLoader_ (ADR-0009); transitions Loading -> Running once it returns.
    virtual void VLoadLevel(const std::string &levelPath);

    // Ticks every attached view's VOnUpdate. Does NOT call VOnRender -- see "Who drives the loop"
    // below for why rendering is deliberately not reachable through BaseGameLogic at all.
    void VOnUpdate(float dt);

    GameLogicState State() const { return state_; }

protected:
    entt::registry &registry_;
    EventManager &events_;
    ProcessManager &processes_;
    LevelLoader &levelLoader_;

private:
    std::vector<std::unique_ptr<IGameView>> views_;
    GameViewId nextViewId_ = 1;
    GameLogicState state_ = GameLogicState::Loading;
};
```

`BaseGameLogic` does **not** own its own `entt::registry`/`EventManager`/`ProcessManager` -- it
takes references to `Engine`'s existing instances (ADR-0001/0003), the same "doesn't own, takes
what it needs per call/construction" shape `LevelLoader` already uses (ADR-0009). `registry_`/
`events_`/`processes_` are already ticked once per frame by `Engine::Run`'s `TickAndUpdateDraw`
(`DispatchQueued`, `ProcessManager::Update`, `PropagateTransforms`) -- `BaseGameLogic` doesn't
re-tick any of them, it just holds references so `VLoadLevel` and future game-specific subclasses
don't need `Engine` threaded through everywhere.

**State machine trimmed to `Loading`/`Running`/`Paused`**, not the book's fuller
`MainMenu`/`WaitingForPlayers`/... states -- `screens.h`'s `GameScreen` enum
(`LOGO`/`TITLE`/`OPTIONS`/`GAMEPLAY`/`ENDING`) already answers the app-level "what screen" question
those book states were partly covering; a `BaseGameLogic` state machine only needs to describe what
happens *inside* the `GAMEPLAY` screen's own simulation.

### 3. Who drives `VOnUpdate`/`VOnRender`, and where `BaseGameLogic` lives

The book's `GameCodeApp` main loop owns `m_pGame` (the one running `BaseGameLogic`) and calls
`VOnUpdate` on it, then `VOnUpdate`/`VOnRender` on every attached view. `Engine` here deliberately
does **not** play that role (ADR-0001 Decision 2's whole point) -- so `BaseGameLogic` needs a
different, non-`Engine` home.

| | **A: `screen_gameplay.c` owns it (chosen)** | **B: `Engine` owns it** |
|---|---|---|
| Respects ADR-0001 Decision 2 | Yes -- `Engine` stays Ch. 5-only, unchanged | No -- reopens a decision already made for a stated reason, not a new one specific to this ADR |
| Matches existing precedent | Yes -- identical shape to how `LevelLoader` (ADR-0009) is meant to be constructed/driven from an `extern "C"` bridge in `InitGameplayScreen()` | N/A -- would need inventing a new integration point |
| Scope of what needs a Logic/View split | Correct -- `LOGO`/`TITLE`/`OPTIONS`/`ENDING` are fixed UI screens with no simulation to decouple Logic from View for; only `GAMEPLAY` has one | Overbroad -- would force every screen through Logic/View machinery it doesn't need |

**Decided: A.** `screen_gameplay.c`'s four functions become the bridge, via the same kind of
`extern "C"` bridge ADR-0009 already anticipates for `LevelLoader`:

- `InitGameplayScreen()` — bridge constructs a `BaseGameLogic`, attaches a `HumanView` (§4), calls
  `VLoadLevel(...)`.
- `UpdateGameplayScreen()` — bridge calls `logic->VOnUpdate(dt)`.
- `DrawGameplayScreen()` — bridge calls the attached view(s)' `VOnRender(dt)` directly, **not**
  through `BaseGameLogic`. Logic never renders, mirroring the book's own separation exactly:
  `BaseGameLogic::VOnUpdate` only reaches views' `VOnUpdate`.
- `UnloadGameplayScreen()` — bridge tears down `BaseGameLogic` and its attached view(s).

`LOGO`/`TITLE`/`OPTIONS`/`ENDING` stay exactly as they are today: plain C, no `BaseGameLogic`, no
`IGameView` -- consistent with ADR-0009's own framing that a menu screen has nothing needing a
Logic/View split.

### 4. `HumanView` — the one concrete `IGameView` built now

The first real implementation, replacing today's placeholder `DrawGameplayScreen` (a static
`DrawRectangle`/`DrawTextEx` stub — see `src/game/screen_gameplay.c`) with an actual render pass:
iterates entities via the registry (a render component's shape is not decided here -- see Open
Questions), draws them with a raylib `Camera3D`, and reads raylib's polled input functions
directly in `VOnUpdate` to drive whatever entity it's attached to (the `actorId` from `VOnAttach`)
-- e.g. writing to that entity's `LocalTransform` (ADR-0002).

| | **A: `VOnMsgProc`-style translation (book's shape)** | **B: poll raylib input directly (chosen)** |
|---|---|---|
| Fits raylib's actual input model | No -- raylib has no message-queue/proc concept; would mean building a translation layer raylib doesn't need | Yes -- `IsKeyDown`/`GetGesture...`/etc. are already exactly this: polled, per-frame input state |
| Extra code | A whole `AppMsg` type + dispatch table, solving a problem that doesn't exist here | None -- `VOnUpdate` just calls raylib's existing functions |

**Decided: B.**

### 5. `RemoteView`/`AIView` — named, not built

`GameViewType::Remote`/`::AI` exist in the enum now specifically so `BaseGameLogic`'s
attach/detach/update machinery never needs to change shape once real implementations land --
the same "keep the seam" reasoning ADR-0009 used for `EvtData_EntitySpawned`. Neither is
implemented here, matching this project's usual discipline for the concrete pieces that *aren't*
the seam itself:

- `RemoteView` needs [ADR-0005](0005-event-manager-queued-dispatch-idata-lua-proposal.md)'s
  `ISerializableEvent`/`EventTypeRegistry` (Proposed, partially implemented) plus an actual network
  transport, which [ADR-0007](0007-terraform-gated-on-authoritative-server.md) confirms doesn't
  exist and isn't decided yet.
- `AIView` needs entities and behavior to decide about (`.claude/skills/engine-ai-behavior`'s
  FSM/utility/steering guidance exists, but zero gameplay entities exist to run it against).

## Tradeoffs accepted

- **Building the split now is a deliberate exception to "wait for a concrete trigger,"** the same
  category of call ADR-0009 made for `EvtData_EntitySpawned` — accepted because `HumanView`,
  `RemoteView`, `AIView`, and game-specific `BaseGameLogic` subclasses are explicitly the
  next-planned work, and every one of them needs this seam to exist first; retrofitting it after
  any get built independently costs more than building it once now.
- **No message-proc translation layer, no device-loss recovery hook** — dropped as book mechanics
  with no raylib/OpenGL analogue at this project's scope; add a hook later only if a concrete
  platform problem (e.g. context loss on some target) actually needs one.
- **`BaseGameLogic`'s state machine is trimmed** to `Loading`/`Running`/`Paused` — the book's fuller
  `MainMenu`/`WaitingForPlayers` states are already `screens.h`'s job; duplicating them inside
  `BaseGameLogic` would be two state machines answering an overlapping question.
- **Only `HumanView` is actually implemented.** `RemoteView`/`AIView` are named in the type enum
  but not built — the interface itself is the deliberate exception above; the concrete
  implementations still follow the project's normal "don't build ahead of need" discipline.
- **Depends on two not-yet-built ADRs** — `LevelLoader` (ADR-0009) and `EntityFactory`
  (ADR-0008), both still `Proposed`. This ADR's own code can't be implemented before at least
  ADR-0009 lands; see Consequences for sequencing.
- **No render component exists yet** for `HumanView` to query entities by — flagged as an Open
  Question, not designed here.

## Consequences / follow-ups

- **Sequencing**: this ADR can be reviewed/accepted now, but implementation should wait for
  [ADR-0008](0008-data-driven-entity-loading-yaml.md) and
  [ADR-0009](0009-level-loading-actor-placement.md) to land first — `BaseGameLogic::VLoadLevel`
  has nothing to call otherwise.
- `docs/roadmap.md`'s "Not started" `BaseGameLogic`/`IGameView` line moves to "Proposed" (this
  ADR), noting its dependency on ADR-0008/0009 landing first.
- `.claude/skills/engine-architecture` should gain a note once any of `IGameView`/`BaseGameLogic`/
  `HumanView` land in code, per that skill's own "update once it lands" convention.
- A render component (what data `HumanView` actually reads to draw an entity — a `Model` handle
  via `ResourceCache<Model>` per ADR-0004, at minimum) needs its own design before `HumanView` can
  draw anything beyond raw `LocalTransform` positions. Not designed here.
- Input/action mapping (which raylib keys/gestures drive which actor action) has no design yet —
  `HumanView` polling raylib input directly in `VOnUpdate` is decided (§4), but the actual
  bindings/action-mapping layer isn't.
- Once a real `RemoteView` gets built, it's the natural subscriber to `EvtData_EntitySpawned`
  (ADR-0009) via ADR-0005's serialization path — this ADR doesn't build that, just confirms the
  seam already lines up for it.
- The extern "C" bridge `screen_gameplay.c` needs to hold `BaseGameLogic`/view instances across
  separate `Init`/`Update`/`Draw`/`Unload` calls — likely a file-local static in the bridge
  translation unit, the same pattern `raylib_game.cpp` already uses for screen-transition state.
  Sketched informally in §3, not fully specified.

## Open Questions

- **Render component design** — what a `HumanView` actually reads per entity to draw it (a `Model`
  handle at minimum; material/animation state later). Not decided here.
- **Input/action mapping** — raw key/gesture → actor action translation. Not decided here.
- **Split-screen / multiple simultaneous `HumanView`s** — `BaseGameLogic::views_` technically
  supports attaching more than one, but camera/viewport partitioning isn't designed.
- **Should `BaseGameLogic` be directly instantiable, or stay abstract until a first game-specific
  subclass exists?** Leaning instantiable-with-virtual-hooks (matches the book's own
  `BaseGameLogic`, a real base class subclasses selectively override, not a pure interface) — not
  firmly decided; revisit once a first concrete game-specific subclass is actually written.
- **Pause behavior** — `GameLogicState::Paused` exists in the state enum, but what pausing actually
  freezes (view updates? `ProcessManager`? both?) isn't specified.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 9-10 — `BaseGameLogic`,
  `IGameView`, `HumanView`/`RemoteView`/`AIView`, the Logic/View split this ADR activates.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) Decision 2 — the original deferral this ADR
  ends, and the reason `Engine` doesn't own any of this.
- [ADR-0003](0003-event-manager-and-process-manager-game-loop.md) — `EventManager`/`ProcessManager`,
  referenced (not owned) by `BaseGameLogic`.
- [ADR-0002](0002-scene-graph-hierarchy-options.md) — `LocalTransform`, what `HumanView` writes to
  when driving a possessed actor.
- [ADR-0005](0005-event-manager-queued-dispatch-idata-lua-proposal.md) — `ISerializableEvent`/
  `EventTypeRegistry`, what a future `RemoteView` would use to forward state across the network.
- [ADR-0008](0008-data-driven-entity-loading-yaml.md), [ADR-0009](0009-level-loading-actor-placement.md)
  — `EntityFactory`/`LevelLoader`, both required before `BaseGameLogic::VLoadLevel` can be
  implemented; the precedent for the `extern "C"` screen-bridge pattern this ADR reuses.
- `.claude/skills/engine-ai-behavior/SKILL.md` — the FSM/utility/steering guidance a future
  `AIView` would apply, once entities exist to apply it to.
- `docs/roadmap.md` — tracks this item's status alongside every other ADR in the project.
