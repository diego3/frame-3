# 13. Input / key-binding system: data-driven action→key mapping for `HumanView`

- Status: Accepted
- Date: 2026-08-05

## Implementation status (2026-08-06)

Landed as designed below, picked up as part of
[RFC-0001](../rfc/0001-flare-reactor-pipeline-experiment.md)'s InputManager step, with two
deviations discovered while implementing:

- **`InputAction` gained a fifth value, `Interact`, beyond this ADR's original movement-only
  sketch.** §1's own Open Questions left "should `InputAction` grow beyond movement... ahead of a
  second game existing, or only once a concrete game needs one?" undecided, leaning toward the
  latter — that's exactly what happened: `game/flare_reactor`'s `PlayerInteractElement`-equivalent
  needed a discrete, non-movement action, so `Interact` was added at that point, not guessed at
  ahead of time.
- **`IsDown` alone wasn't enough — `IsPressed` (edge-triggered, wraps raylib's `IsKeyPressed`) was
  added alongside it.** This ADR's original §4 sketch only ever needed level-triggered movement
  (`IsKeyDown`); a discrete one-shot action like `Interact` firing every frame the key stays held
  (what `IsDown` alone would do) is wrong for a "press E to activate" action — a real gap this
  ADR's own design left uncovered, not anticipated in the Decision above. Both `IsDown`/`IsPressed`
  are defined `inline` directly in `input_bindings.h`, not `input_bindings.cpp` — deliberately, so
  the `.cpp` (also compiled into the test build, `tests/input_bindings_test.cpp`) never references
  `IsKeyDown`/`IsKeyPressed` and stays link-safe without `-lraylib`, the same
  raylib-runtime-vs-headers-only split `src/Makefile` already documents for
  `game/sandbox/human_view.cpp`.

What actually shipped: `InputAction`/`InputBindings`/`LoadOrCreateInputBindings`
(`src/app/input_bindings.h`/`.cpp`), first real (non-test) caller
`game/flare_reactor/human_view.cpp`'s `FlareReactorView` — its constructor calls
`LoadOrCreateInputBindings()` with no path argument, same as the sketch's default
(`config/keybindings.yaml`, confirmed landing at `src/config/keybindings.yaml` on first run,
already covered by the existing `/src/config/` `.gitignore` entry). `VOnUpdate`'s four movement
checks now read `input_.IsDown(InputAction::MoveForward/...)` instead of raw `IsKeyDown`, matching
§4's sketch exactly (axis mapping included); `Interact` is read via `IsPressed` and, for now, only
`TraceLog`s — no `EvtData_ActivateBeacon`/`GameLogic` handler exists yet (RFC-0001 Phase 2).
**`game/sandbox/human_view.cpp` was NOT migrated** — it still reads `IsKeyDown(KEY_RIGHT)` etc.
directly; a natural follow-up, not required for this ADR to be real, and left for whenever sandbox
itself needs rebinding. `tests/input_bindings_test.cpp` exercises the
`LoadOrCreateInputBindings`/`KeyFor` round trip (defaults, existing file, missing-field fallback) —
`IsDown`/`IsPressed` themselves aren't unit-tested, same limitation `HumanView`'s own input handling
already has (calling real raylib functions means nothing meaningful without a window/GL context;
verified instead by a headless `xvfb-run` smoke test pressing through several seconds with `E`'s
`TraceLog` firing and no crash).

Gamepad support and a rebinding UI remain exactly as deferred as the Tradeoffs section below always
said — nothing changed there.

## Context

[ADR-0010](0010-base-game-logic-and-igameview.md) §4 decided how `HumanView` reads input at all:
poll raylib's input functions directly in `VOnUpdate` (`IsKeyDown`/`GetGesture...`/etc.), not a
Win32-style `VOnMsgProc(AppMsg)` translation layer — raylib already hands out polled per-frame
input state, so there's nothing to translate. That ADR explicitly did **not** decide the next
question, listing it verbatim under its own Open Questions: "**Input/action mapping** — raw
key/gesture → actor action translation. Not decided here." What actually shipped
(`src/app/human_view.cpp`) is a hardcoded placeholder, flagged in its own comment:

```cpp
// Placeholder input scheme -- raw key/gesture to actor-action mapping is an open question
// (ADR-0010's own Open Questions, still not designed here); this hardcodes arrow keys to X/Z
// movement so there's something to observe, not a real bindings layer.
constexpr float kMoveUnitsPerSecond = 4.0f;
```

with `VOnUpdate` itself calling `IsKeyDown(KEY_RIGHT)`/`KEY_LEFT`/`KEY_DOWN`/`KEY_UP` directly and
adding to a movement vector inline. There is no concept of an "action" anywhere in the code today
— the arrow keys and the X/Z movement they produce are the same thing, wired together in one
function body. This ADR is that missing piece: a real seam between "what the player pressed" and
"what the possessed actor does about it," with the key side of that seam data-driven and
rebindable, per [`docs/roadmap.md`](../roadmap.md)'s own framing of this item.

**Distinct from [ADR-0011](0011-engine-and-game-config.md)**, which `docs/roadmap.md` already
calls out explicitly: `EngineConfig`/`GameConfig` cover display/audio/window settings — parameters
`Engine::Init()` itself consumes. Key bindings are a different axis entirely: which physical input
triggers which in-game action, consumed by `HumanView`, not by `Engine`. Both happen to share the
same "writable file outside `assets/`, generated with defaults on first run" shape, and this ADR
reuses that shape deliberately (see Decision §2) — but they are two separate files, two separate
concerns, matching the roadmap's own separation.

**Distinct from `GameConfig`** too: `GameConfig` (per ADR-0011) is explicitly *per-game* — owned by
whatever specific game runs on frame-3, currently empty because this raylib-template stub game has
no game-specific settings yet. Input bindings are not game-specific in that sense: any game built
on `HumanView` still needs a player-possessed actor to move via *some* set of keys, and that
mapping should be reassignable regardless of which game is running. This lives alongside
`HumanView`/`game_view.h` in `src/app/`, not in `src/game/`.

**Dependency**: this ADR is written directly against `HumanView`/`BaseGameLogic`/`IGameView`
(ADR-0010), which exist only on the still-open `claude/base-game-logic-impl` branch (PR #26) —
`main` still shows ADR-0010 as `Proposed`. This ADR's branch is based on that branch, and its PR
targets it rather than `main`, the same pattern ADR-0009 followed when it depended on ADR-0008's
then-open branch.

## Decision

### 1. A fixed `InputAction` enum — the *mapping*, not the action set, is data-driven

```cpp
// input_bindings.h (sketch)
enum class InputAction {
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
};
```

|  | **A: fully data-driven action set (string-keyed, actions defined in the same file as the mapping)** | **B: fixed enum, only the key side is data-driven (chosen)** |
|---|---|---|
| What a new action requires | Editing a YAML file only — but every consumer (`HumanView::VOnUpdate`) still needs a matching code path to *do* something for that action, so this saves nothing in practice | A one-line enum addition plus whatever `VOnUpdate` code interprets it — same total work, less indirection |
| Matches how `HumanView` already consumes input | No — `VOnUpdate` doesn't currently do a lookup for "whatever actions exist," it has fixed movement logic per direction | Yes — `HumanView` already knows semantically what "move forward" means; only *which key* triggers it needs to be player-configurable |
| Precedent in this project | None — nothing else treats a fixed piece of view-code behavior as itself data | Matches `EntityFactory`'s component *loaders* being fixed code registered by name (ADR-0008) while the *data* driving them (YAML) is what's swappable |

**Decided: B.** The set of actions a view understands is inherent to that view's code, the same
way `EntityFactory`'s `"Position"` loader (ADR-0010's implementation) is a fixed piece of C++ that
a data file merely *invokes* by name, not something the data file defines from scratch. Adding a
new action (e.g. `Jump`) is a normal code change to the enum plus whatever `VOnUpdate` logic
interprets it — not different in kind from adding any other component loader or behavior in this
project. What's genuinely player-configurable, and therefore what this ADR makes data-driven, is
strictly narrower: *which physical key currently triggers an already-existing action.*

### 2. Binding storage: reuses ADR-0008's read path, ADR-0011's write/first-run pattern

```cpp
// input_bindings.h (sketch)
class InputBindings {
public:
    // key codes are raylib's own KEY_* integer constants (see §"Raw int key codes" below).
    bool IsDown(InputAction action) const;

private:
    std::unordered_map<InputAction, int> keys_;
    friend InputBindings LoadOrCreateInputBindings(const std::string &path);
};

// Reads path; if it doesn't exist yet (first run), returns the defaults below (matching
// HumanView's current arrow-key scheme exactly, so behavior doesn't silently change the moment
// this ADR lands) and writes them out to that path -- same "first run creates the file" pattern
// LoadOrCreateEngineConfig (ADR-0011) already established. A binding missing from an existing
// file (e.g. written by an older build before a new InputAction existed) keeps its default,
// mirroring EngineConfig's own missing-field handling.
InputBindings LoadOrCreateInputBindings(const std::string &path = "config/keybindings.yaml");
```

The on-disk format is read via `YamlEntityFileParser`/`EntityDefNode` (ADR-0008) exactly like
`EngineConfig` does — `AsInt()` per action key, falling back to the default on a missing or
malformed entry. Writing reuses the same "minimal, hand-written emitter scoped to the actual
shape" call ADR-0011 made for `SerializeEngineConfig`, here emitting one `ActionName: <int>` line
per binding:

```yaml
# config/keybindings.yaml (generated on first run)
MoveForward: 265   # KEY_UP
MoveBackward: 264  # KEY_DOWN
MoveLeft: 263       # KEY_LEFT
MoveRight: 262      # KEY_RIGHT
```

**Raw int key codes, not human-readable names** (`"UP"`/`"Arrow Up"`) — a deliberate simplicity
tradeoff, not an oversight; see Tradeoffs.

`config/keybindings.yaml` lives at `src/config/` alongside `config/engine.yaml`/`config/game.yaml`
— already covered by the existing `/src/config/` `.gitignore` entry (ADR-0011), no new ignore rule
needed.

### 3. Ownership: `HumanView` loads and owns its own `InputBindings`

|  | **A: `HumanView` loads/owns it (chosen)** | **B: `BaseGameLogic` loads it, passes it to attached views** |
|---|---|---|
| Matches which views actually need it | Yes — only `HumanView` polls raylib input at all (ADR-0010 §4); a `RemoteView`/`AIView` has no keyboard to bind | No — routes a `HumanView`-specific concept through the generic base class's attach/update path, which every other view type would have to ignore |
| Extra plumbing | None — mirrors `Engine::Init()` calling `LoadOrCreateEngineConfig()` itself | `BaseGameLogic::AttachView`/constructor would need a new parameter or lookup path nothing else needs |
| Cost if a second `HumanView` (split-screen, ADR-0010's own Open Question) ever exists | Each instance loads/owns its own bindings independently — fine today; revisit if split-screen ever needs distinct per-player bindings, which this shape already supports for free | Same open question either way |

**Decided: A.** `HumanView`'s constructor calls `LoadOrCreateInputBindings()` itself, the same
"own what only I need" shape `Engine::Init()` already uses for `EngineConfig`.

### 4. `HumanView::VOnUpdate` change

The four inline `IsKeyDown` calls are replaced by asking `InputBindings` per action:

```cpp
// human_view.cpp (sketch)
Vector3 move{0.0f, 0.0f, 0.0f};
if (input_.IsDown(InputAction::MoveForward))  move.z -= 1.0f;
if (input_.IsDown(InputAction::MoveBackward)) move.z += 1.0f;
if (input_.IsDown(InputAction::MoveLeft))     move.x -= 1.0f;
if (input_.IsDown(InputAction::MoveRight))    move.x += 1.0f;
```

`IsDown(action)` itself is the one place `IsKeyDown` gets called now — a raylib call wrapped
exactly once, behind the action, instead of scattered across `VOnUpdate` directly. The rest of
`VOnUpdate` (`Vector3Add`/`Vector3Scale` against `kMoveUnitsPerSecond * dt`, camera-follow) is
unaffected — this ADR only replaces *how the move vector's components get set*, not what happens
with the result.

### Gamepad: named in the roadmap item, explicitly out of scope here

`docs/roadmap.md`'s own phrasing for this item is "action↔key/gamepad-button mapping" — this ADR
builds the keyboard half only. Nothing in this project polls a gamepad anywhere today, and nothing
has asked for one concretely; building it now would be guessing ahead of a need, the same
discipline `RemoteView`/`AIView` were held to (ADR-0010 §5: named in a type enum, not built).
`std::unordered_map<InputAction, int>` storing a raylib key code per action is a mechanical,
non-breaking base to extend later — e.g. a second optional gamepad-button map, or widening the
value type to a small struct — once a real gamepad target exists to test against; not designed
here.

### Rebinding UI: not built here either

Matches ADR-0011's own precedent for `screen_options.c` exactly: that ADR made `EngineConfig`/
`GameConfig` real, persisted files without building any UI to read/write them at runtime.
`screen_options.c` stays exactly the `TODO` stub it is today. This ADR makes the *binding data*
real, persisted, and consumed by `HumanView` — a UI that lets a player press a key to rebind an
action, and calls whatever writes `config/keybindings.yaml` back out, is separate, future work.

## Tradeoffs accepted

- **Fixed `InputAction` enum, not a fully data-driven action set** — accepted because the action
  set is inherently tied to code that interprets it (`HumanView::VOnUpdate`); making the *set* data
  wouldn't remove that coupling, only add indirection for no real gain (§1).
- **Raw raylib integer key codes in the YAML file, not human-readable names** (`"UP"`, `"Arrow
  Up"`) — no name↔keycode lookup table exists anywhere in this project; building one now serves
  only a rebinding UI that doesn't exist yet. Revisit once a real UI needs to *display* a bound
  key, not just store it.
- **Keyboard only, no gamepad support** — named in the roadmap item's own phrasing but explicitly
  deferred; no gamepad code or testing exists anywhere in this project today. The chosen storage
  shape (a map keyed by action) extends to a second gamepad map without breaking this design.
- **No rebinding UI** — same category of deferral ADR-0011 already made for `screen_options.c`;
  this ADR makes the mapping real and persisted, not player-editable through any in-game screen.
- **`HumanView` owns its own `InputBindings` rather than it being threaded through
  `BaseGameLogic`** — keeps a `HumanView`-specific concept out of the generic base class, at the
  cost of each concrete `IGameView` needing to own its own input scheme independently if it has
  one. Fine today since `HumanView` is the only view that polls input at all.
- **Depends on ADR-0010's code**, which exists only on an unmerged branch (PR #26) at the time
  this ADR is written — this ADR can be reviewed/accepted on its own merits, but its code can't
  land before ADR-0010's does, the same sequencing ADR-0010 itself accepted relative to
  ADR-0008/0009.

## Consequences / follow-ups

- `docs/roadmap.md`'s "Input / key-binding system" line moves to "Shipped" (this ADR, now
  `Accepted`) — see Implementation status above for what actually landed and what's still deferred.
- `HumanView::VOnUpdate`'s (`game/sandbox`) four hardcoded `IsKeyDown` calls are **not yet**
  replaced — only `game/flare_reactor`'s `FlareReactorView` uses `InputBindings` so far (see
  Implementation status). Migrating sandbox is a natural, not-yet-scheduled follow-up.
- `.claude/skills/engine-architecture` §9 (added for ADR-0010's `HumanView`) should gain a note
  about `InputBindings`/`InputAction`, per that skill's own "update once it lands" convention —
  not yet done.
- `config/keybindings.yaml` joins `config/engine.yaml`/`config/game.yaml` as a first-run-generated
  file under `src/config/` — already covered by the existing `/src/config/` `.gitignore` entry, no
  new ignore rule needed. Confirmed landing there in practice (see Implementation status).
- A future rebinding UI, gamepad support, and a human-readable name↔keycode table are all natural
  extensions of this shape — none designed here (see Tradeoffs, Open Questions).

## Open Questions

- ~~**Should `InputAction` grow beyond movement** (e.g. `Interact`, `Jump`) ahead of a second game
  existing, or only once a concrete game needs one?~~ **Resolved** (see Implementation status):
  once a concrete consumer (`game/flare_reactor`) needed `Interact`, it was added then — not
  guessed at ahead of time, confirming the "leaning toward the latter" guess this question
  originally left open.
- **Gamepad button mapping** — deferred entirely; see Tradeoffs. Not designed here.
- **Human-readable key names**, needed only once a real rebinding UI exists to display them — not
  designed here.
- **Multiple physical keys bound to one action** (e.g. both WASD and arrow keys triggering the
  same `InputAction` simultaneously) — this design is one key per action; whether that's ever
  actually needed isn't decided.
- **Should split-screen (ADR-0010's own Open Question — multiple simultaneous `HumanView`s) share
  one `InputBindings` or each get an independent one?** This ADR's per-`HumanView`-instance
  ownership (§3) already supports independent bindings per instance for free, but nothing has
  exercised that yet.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 9-10 — the same chapters ADR-0010
  adapts `IGameView`/`HumanView` from; this ADR fills the "input/action mapping" gap that adaptation
  explicitly left open rather than guessed at.
- [ADR-0010](0010-base-game-logic-and-igameview.md) — `IGameView`/`HumanView`/`BaseGameLogic`, the
  direct dependency this ADR builds on; its own Open Questions is where this item originates
  verbatim.
- [ADR-0008](0008-data-driven-entity-loading-yaml.md) — `EntityDefNode`/`YamlEntityFileParser`,
  reused here for reading `config/keybindings.yaml`.
- [ADR-0011](0011-engine-and-game-config.md) — `LoadOrCreateEngineConfig`'s first-run/write pattern
  and `src/config/` as the writable, unversioned config location, both reused directly here; also
  the ADR `docs/roadmap.md` explicitly distinguishes this item from.
- [`docs/roadmap.md`](../roadmap.md) — tracks this item and its dependency on ADR-0010 landing in
  code.
