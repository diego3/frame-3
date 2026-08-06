# 16. `IScreenElement` stack: `HumanView` composes layered, z-ordered elements

- Status: Accepted
- Date: 2026-08-05

## Context

Today, everything a screen shows is one monolithic function. `game/sandbox/screen_gameplay.cpp`'s
`DrawGameplayScreen` calls `HumanView::VOnRender` (which itself hardcodes a 3D camera pass —
`BeginMode3D`, wireframe boxes, a grid — directly in one method) and then bolts on a raw
`DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", ...)` call right after it, outside
`HumanView` entirely. There is no way to add, remove, hide, or reorder a piece of UI without
editing that one function's body, and no shared concept a HUD, a pause menu, and the 3D scene
itself could all implement the same way. `DebugOverlay` (`app/debug_overlay.h`/`.cpp`, the F3
stats HUD) is a third, unrelated mechanism again — free functions called directly from
`main.cpp`'s `UpdateDrawFrame`, spanning every screen rather than belonging to any one view.

This is exactly the gap `docs/roadmap.md`'s "UI/HUD as a system" item already names: *"the book's
`IScreenElement` stack... what exists today is just the raylib template's
logo/title/gameplay/ending/options screen state machine, not a real UI system."* That roadmap
entry also already recorded why it matters beyond tidiness: the book's own `GCC4::HumanView`
(`Source/GCC4/UserInterface/HumanView.h`, in the local gamecode4 reference) is a generic base
class built *around* this stack — `ScreenElementList m_ScreenElements`, `VPushElement`/
`VRemoveElement`, and a `VLoadGameDelegate` hook whose default implementation is just
`VPushElement(m_pScene)`. `TeapotWarsHumanView` (`Source/TeapotWars/TeapotWarsView.cpp`) overrides
that hook to additionally push its own `StandardHUD`. Our `HumanView`
([ADR-0010](0010-base-game-logic-and-igameview.md), living in `game/sandbox/` per
[ADR-0014](0014-game-module-boundary-and-template-migration.md)) has no such stack, so — as that
roadmap note put it — it has nothing generic to leave behind in `app/` yet; it's playing
`TeapotWarsHumanView`'s role, not `GCC4::HumanView`'s. This ADR is what would change that: it
builds the stack, not the generic base itself (see Decision below for why those stay separate).

Not the same concept as `BaseGameLogic`'s coarse `GameLogicState` (`Loading`/`Running`/`Paused`) or
`main.cpp`'s five-screen `GameScreen` enum (`LOGO`/`TITLE`/.../`ENDING`) — the book keeps an
analogous distinction (`BaseGameState`: `BGS_MainMenu`/`BGS_Running`/...). Those answer "which
screen is active"; `IScreenElement` answers "what's layered on top of each other *within* whichever
screen is active" — a HUD element and a pause-menu element can both be visible during `GAMEPLAY` at
once, sorted by z-order, each independently updated/rendered/shown/hidden. This ADR doesn't touch
the five-screen state machine at all.

## Decision

### `IScreenElement` and its container are game-agnostic — they belong in `app/`

Same reasoning [ADR-0014](0014-game-module-boundary-and-template-migration.md) already applied to
`IGameView`/`BaseGameLogic`: a screen element is a generic "update, render, show/hide, z-order"
contract with zero game-specific knowledge. It belongs in `app/`, reusable by any future game
module, the same way `IGameView` already is.

```cpp
// app/screen_element.h (as implemented)
class IScreenElement {
public:
    virtual ~IScreenElement() = default;

    virtual void VOnUpdate(float dt) = 0;
    virtual void VOnRender(float dt) = 0;

    virtual int VGetZOrder() const = 0;
    virtual void VSetZOrder(int zOrder) = 0;
    virtual bool VIsVisible() const = 0;
    virtual void VSetVisible(bool visible) = 0;
};

using ScreenElementId = std::uint32_t;
```

Dropped from the book's `IScreenElement` (`GameCode4/interfaces.h`), same category of cut ADR-0010
already made for `IGameView` and for the same reasons:

- **`VOnRestore()`/`VOnLostDevice()`** — DirectX device-loss recovery; no raylib/OpenGL analogue to
  recover through.
- **`VOnMsgProc(AppMsg)`** — raylib has no Win32 message queue to translate from; input is already
  polled state (`IsKeyDown`/etc.), read directly by whatever `VOnUpdate` needs it, same as
  `IGameView` already decided.
- **The book's `operator<` comparator on the interface itself** — replaced by sorting on
  `VGetZOrder()` where the container sorts (below), not a method every element must carry.

### `HumanView` owns the stack; the container mirrors `BaseGameLogic::views_`, not the book's `std::list<shared_ptr<...>>`

```cpp
// human_view.h addition (as implemented)
class HumanView : public IGameView {
public:
    ScreenElementId PushElement(std::unique_ptr<IScreenElement> element);
    void RemoveElement(ScreenElementId id);
    // ...
private:
    // Paired with its id, unlike BaseGameLogic::views_'s plain vector<unique_ptr<IGameView>> --
    // IGameView self-reports its id (GetId(), set via VOnAttach's own contract), but IScreenElement
    // has no such attach step to hang an id on, so the id has to live beside the element instead of
    // inside it. See screen_element.h's own comment on ScreenElementId for why that's fine.
    std::vector<std::pair<ScreenElementId, std::unique_ptr<IScreenElement>>> elements_;
    ScreenElementId nextElementId_ = 1;
};
```

The book's `ScreenElementList` is `std::list<shared_ptr<IScreenElement>>` — `shared_ptr` because
`TeapotWarsHumanView` keeps its own `m_StandardHUD` member *and* pushes that same `shared_ptr` onto
the list, needing two live references to one object. Nothing in this project has that need yet, and
this codebase already has an established, single-owner shape for exactly this situation:
`BaseGameLogic::views_` is `std::vector<std::unique_ptr<IGameView>>` with an id-based
`AttachView`/`DetachView` pair (`GameViewId`), and `screen_gameplay.cpp` already gets a second,
*non-owning* reference to a `unique_ptr`-owned object by keeping `g_humanView = humanView.get()`
before moving ownership into `AttachView`. `PushElement`/`RemoveElement(id)` above is the same
shape applied to screen elements — a deliberate deviation from the book's ownership choice, not an
oversight: the book predates widespread `unique_ptr`+move (its own C++03/early-C++11 codebase
leans on `shared_ptr` broadly); modern C++20 covers the actual single-owner need here without
refcounting.

`VOnUpdate`/`VOnRender` iterate `elements_`, skipping any where `!VIsVisible()`. `VOnRender`
`std::stable_sort`s by `VGetZOrder()` first (ascending — lower z-order renders first, so a higher
z-order element visually layers on top of it), mirroring the book's own
`m_ScreenElements.sort(SortBy_SharedPtr_Content<IScreenElement>())` before its render pass.
`VOnUpdate` doesn't need the sort (order-independent).

### First two elements, so the stack has real, non-hypothetical consumers

Matches this project's own repeated discipline (`EvtData_EntitySpawned` firing unconditionally
before a real subscriber existed, ADR-0009; `EvtData_CollisionBegin`/`End` designed the same way,
ADR-0012) — build the seam against a concrete first use, not a fake:

1. **The 3D scene itself becomes a screen element.** `HumanView::VOnRender`'s current direct
   `BeginMode3D`/wireframe-box/`DrawGrid` body moves into a concrete `GameplayScene :
   IScreenElement`, pushed once in `HumanView`'s constructor (or `VOnAttach`). This is a pure move,
   not a behavior change — same rendering, now reachable as an element instead of hardcoded.
2. **The "PRESS ENTER..." prompt becomes a HUD element.** `screen_gameplay.cpp`'s
   `DrawGameplayScreen`-level `DrawText(...)` call — currently *outside* `HumanView` entirely —
   moves into a small `GameplayHud : IScreenElement` pushed the same way, mirroring
   `TeapotWarsHumanView::VLoadGameDelegate` pushing `StandardHUD` alongside the base scene. This is
   what actually exercises z-ordering and multi-element composition, not just single-element
   indirection through the same interface.

### `DebugOverlay` stays outside this stack

`DebugOverlay` is app-global — it spans every screen (`LOGO` through `ENDING`), not just whichever
view is active, and is driven from `main.cpp`'s frame loop, not from a `HumanView`. Folding it into
`HumanView`'s element stack would only work while `GAMEPLAY` is the active screen, which is a
regression from what it does today. It keeps its existing free-function shape; noted here so this
ADR doesn't leave that as an unexplained gap.

### What this ADR deliberately does *not* do

**No generic `HumanView` base class promoted to `app/`.** That's the roadmap note's own framing:
promoting the base (with a `VLoadGameDelegate`-style hook) only makes sense once a second game
creates evidence for what's actually generic — the same "don't build ahead of need" gate
[ADR-0014](0014-game-module-boundary-and-template-migration.md) and
[ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md) already apply elsewhere.
This ADR builds the stack the eventual hook would sit on top of; it doesn't build the hook or the
base class itself. `HumanView` — concrete, `PushElement` calls and all — stays in `game/sandbox/`.

## Tradeoffs accepted

- **No `BaseUI`-style convenience base class yet** (the book's `BaseUI : public IScreenElement`,
  factoring out `VIsVisible`/`VSetVisible`/position/size boilerplate for dialog-shaped elements).
  `GameplayScene` and `GameplayHud` are different enough shapes that a shared base isn't obviously
  right yet — add one once a third element shows the actual duplication, not before.
- **No console** (the book's `HumanView::Console`, a runtime script-command entry field) — no
  scripting layer exists to attach it to ([ADR-0005](0005-event-manager-queued-dispatch-idata-lua-proposal.md)
  §7 already rejected Lua for the same reason); revisit only if that changes.
- **`std::vector<std::unique_ptr<...>>`, not the book's `std::list<shared_ptr<...>>`** — cheaper
  for the small element counts expected here, at the cost of O(n) `RemoveElement` instead of O(1);
  not worth a `std::list`'s pointer-node overhead unless a screen ever hosts enough elements for
  that to matter, which nothing here does.
- **`VOnMsgProc` dropped, so there's no reverse-order "topmost element eats input first" pass** the
  book's message-proc loop gives elements for free. Nothing pushed onto the stack today needs
  input priority over another element (only `HumanView::VOnUpdate` itself reads input, directly);
  revisit if a modal element (a pause menu blocking gameplay input) becomes a real case.

## Consequences / follow-ups

- **What actually shipped, alongside this ADR**: `app/screen_element.h` (`IScreenElement`,
  `ScreenElementId`); `HumanView::PushElement`/`RemoveElement` and its `elements_` member
  (`human_view.h`/`.cpp`); `GameplayScene` and `GameplayHud` (file-local classes in
  `human_view.cpp` — nothing outside that file names them, so no header of their own). `HumanView`'s
  old direct-draw `VOnRender` body and `screen_gameplay.cpp`'s standalone `DrawText` call are both
  gone, replaced by the two elements above. Verified: full build (`-Werror`) and all 83 unit tests
  passing, plus a headless `xvfb-run` smoke test that jumped straight to `GAMEPLAY` (bypassing the
  `LOGO`/`TITLE` screens' own timing) and ran several hundred frames with both elements
  updating/rendering, no crash.
- `docs/roadmap.md`'s "UI/HUD as a system" item moves to "Shipped", noting what's still open (see
  Tradeoffs above — no `BaseUI`-equivalent, no console, no modal input priority).
- `.claude/skills/engine-architecture/SKILL.md` gains a section for `IScreenElement`, per that
  skill's own "update once it lands" convention.
- Once a second game exists ([ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md)'s
  own trigger), *that's* the point to revisit promoting a generic `HumanView` base with a
  `VLoadGameDelegate`-style hook into `app/`, now that the stack it would sit on already exists.

## Open Questions

- **Z-order collisions** (two elements with the same `VGetZOrder()`) — `std::stable_sort` makes
  insertion order the tiebreaker, not designed further than that; revisit if a real ordering bug
  ever surfaces from it.
- **Does `ProcessManager`/audio (HumanView's own still-unused constructor dependencies, see
  `human_view.h`) end up driving anything through these elements** — e.g. a fade-in `Process`
  toggling a HUD element's `VSetVisible`? Not designed here; noted because it's the first plausible
  concrete use for both.
- **Should `GameplayScene`'s render component design finally get decided here** (it currently draws
  a hardcoded wireframe box per `WorldTransform`, an open question since ADR-0010)? Leaning no —
  moving the *existing* rendering into an element is this ADR's job; redesigning what it renders is
  a separate, still-open question this ADR doesn't need to resolve to be useful.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 10 — `IScreenElement`,
  `ScreenElementList`, `HumanView::VPushElement`/`VOnRender`'s sort-and-dispatch pass.
- Local reference: `Source/GCC4/GameCode4/interfaces.h` (`IScreenElement`, `ScreenElementList`
  typedef), `Source/GCC4/UserInterface/HumanView.h`/`.cpp` (`VPushElement`/`VRemoveElement`,
  `VOnRender`'s sort, `VLoadGameDelegate`), `Source/GCC4/UserInterface/UserInterface.h` (`BaseUI`,
  `ScreenElementScene`), `Source/TeapotWars/TeapotWarsView.cpp` (`TeapotWarsHumanView::
  VLoadGameDelegate` pushing `StandardHUD` alongside the base scene).
- [ADR-0010](0010-base-game-logic-and-igameview.md) — `IGameView`/`HumanView`, and the precedent
  for which book-specific machinery gets dropped adapting to raylib.
- [ADR-0014](0014-game-module-boundary-and-template-migration.md) — the `app/` vs `game/` boundary
  this ADR's `IScreenElement` placement follows.
- [ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md) — the "gated on a second
  consumer" pattern this ADR applies to *not* promoting a generic `HumanView` base yet.
