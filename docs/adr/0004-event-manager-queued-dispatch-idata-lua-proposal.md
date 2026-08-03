# 4. Event manager: evaluating the book's `IEventData`/GUID/queued-dispatch/Lua proposal

- Status: Proposed
- Date: 2026-08-03

## Context

[ADR-0002](0002-event-manager-and-process-manager-game-loop.md) already shipped `EventManager`
(`src/app/event_manager.h`): pub/sub keyed by `std::type_index`, handlers are
`std::function<void(const T&)>`, and `Emit<T>` dispatches **synchronously and immediately** — no
queue, no per-frame budget. That ADR explicitly chose this over the book's GUID+monolithic-enum
multicast mechanism, reasoning that `std::type_index` dispatch doesn't carry the lookup/dispatch
cost the book's `VTick(20ms)` budget was defending against. `Engine` (`src/app/engine.h`) owns the
one instance, exposed via `Events()`. As of ADR-0002, nothing in the codebase calls `Subscribe`
or `Emit` yet — no event struct types exist either.

A follow-up proposal asks for "more detail" on the event manager, laying out *Game Coding
Complete* Ch. 4's fuller design: an `IEventData` interface (event type, timestamp, serialization)
with a `BaseEventData` helper base, GUID-based type identification, two dispatch modes —
`VTriggerEvent` (immediate) and `VQueueEvent` (deferred to next frame, via two alternating
queues so that processing one event can safely queue new ones without looping infinitely or
processing "this frame's" events twice) — and a Lua scripting bridge (`QueueEvent`/
`RegisterEventListener` callable from script).

This ADR evaluates each piece of that proposal against what ADR-0002 already shipped and decided,
rather than accepting or rejecting the proposal as a single block — some pieces solve a real gap
in the current design, others re-open ground ADR-0002 already covered for a stated reason, and one
has no target to integrate with in this codebase at all.

## Evaluation, piece by piece

### 1. GUID (32-bit integer) + enum type identification — reject, no new reasoning to add

This is exactly the mechanism ADR-0002 already considered and passed over: *"porting the GUID+enum
mechanism literally would reintroduce exactly the 'terrible monolithic enumeration' problem the
book itself was working around."* The book's GUID exists to give event types a stable identity
that survives separate compilation units and, notably, **network wire compatibility** across
client/server binaries built at different times — neither applies here. `std::type_index` already
gives every event type a unique, collision-free identity with zero registry to maintain, for a
single local C++ binary. Nothing in this proposal introduces a reason (networking, save-file
versioning, cross-binary ABI) to revisit that call.

### 2. `IEventData`/`BaseEventData` interface (type, timestamp, `VCopy`, `GetName`) — reject as a mandatory base class; timestamp opt-in per event

Today an event is just a plain struct — `Emit<T>` requires nothing of `T` beyond being copyable
into the closure captured at `Subscribe<T>` time. Forcing every event to inherit from `IEventData`
and implement four virtual methods (`VGetEventType`, `VGetTimeStamp`, `VCopy`, `GetName`) would
undo the exact benefit ADR-0002 chose templates for: "adding a new event kind is just defining a
new struct" becomes "adding a new event kind is defining a struct *and* implementing a fixed
virtual interface" — a smaller, but real, version of the same boilerplate-per-event-type problem.

- `VGetEventType()` — redundant; `std::type_index(typeid(T))` already is the type identity,
  computed by the template, not something each event class needs to hand-implement.
- `VCopy()`/polymorphic cloning — exists in the book because events travel through
  `shared_ptr<IEventData>` and sometimes need duplicating across the two alternating queues
  (see §3). If queued dispatch is adopted, plain `std::function`-captured-by-value semantics (a
  `T` copy, same as `Emit<T>` already does) cover this without a virtual `VCopy()`.
  Update per the proposal below: no runtime dispatch of the correct cloning code is truly needed
  here.
- `GetName()` (debug logging) — `typeid(T).name()` is not portably pretty, but is already
  available for free without an interface method; not worth a virtual call for this alone.
- **Timestamp** is the one piece with standalone value (e.g. measuring latency between an event
  firing and a handler reacting to it, or ordering events that arrive the same frame). Recommend:
  add a `float timestamp` field to an individual event struct only when a concrete consumer needs
  it, the same "define what you need, when you need it" pattern already used for event types
  themselves — not a `BaseEventData` every event is forced to inherit.

### 3. `VTriggerEvent` (immediate) vs. `VQueueEvent` (deferred, double-buffered) — adopt the queued path as an addition

This is the one piece of the proposal that closes a real gap. `Emit<T>` today iterates
`handlers_[type]` synchronously; nothing stops a handler from re-entering the same `EventManager`
mid-dispatch — e.g. a handler for `EvtData_Destroy_Actor` that itself calls
`Subscribe<EvtData_Destroy_Actor>` or `Emit<EvtData_Destroy_Actor>` while `handlers_[type]` is
still being iterated. A `Subscribe` call that grows the vector being iterated can invalidate the
iterators the range-based `for` in `Emit` is using — undefined behavior, not just "surprising
order." Nothing has hit this yet only because nothing calls `Subscribe`/`Emit` at all yet (per
ADR-0002's own note) — which makes now, before real call sites exist and start depending on
today's implicit immediate-only semantics everywhere, the right time to decide this rather than
retrofitting it later.

The book's fix is two alternating queues: events queued during processing of "this frame's" queue
land in the *other* queue, then the two are swapped — so a cascade of events triggered by
processing never gets processed in the same pass that produced it, and never loops forever.

**Decision: add `Queue<T>`/`DispatchQueued()` alongside the existing `Emit<T>`, not instead of
it.**

```cpp
// Addition to EventManager (sketch).
template <typename T>
void Queue(T event) {
    pending_.push_back([this, event = std::move(event)] { Emit(event); });
}

// Called once per frame from Engine::Run(), the same place ProcessManager::Update() is already
// ticked (src/app/engine.cpp's TickAndUpdateDraw trampoline).
void DispatchQueued() {
    std::vector<std::function<void()>> active;
    std::swap(active, pending_);   // anything Queue()'d *during* this loop lands in the now-empty
    for (auto &dispatch : active) dispatch();   // pending_, not in `active` -- no re-entrant loop
}
```

- `Emit<T>` stays exactly as ADR-0002 shipped it — a genuinely immediate call, for the rare case
  that truly needs same-frame synchronous delivery.
- `Queue<T>` becomes the default recommendation for cross-system events (matches the book's own
  usage pattern, where `VQueueEvent` is used far more often than `VTriggerEvent` in practice) —
  the case this ADR's §3 gap analysis is actually about.
- `DispatchQueued()` is ticked from `Engine::Run()`'s `TickAndUpdateDraw` trampoline
  (`src/app/engine.cpp`), alongside `Processes().Update(GetFrameTime())`, so both frame-paced
  systems are driven from the same place.
- **No time budget (`VTick(20ms)`) added here either** — ADR-0002's reasoning for that still
  holds (`std::type_index` dispatch doesn't carry the cost the budget defended against). The
  double-buffer swap is solving a *correctness* problem (reentrancy/iterator invalidation), not a
  *cost* problem — the two are separable pieces of the book's design, and this ADR only adopts the
  former.

### 4. Lua scripting bridge (`QueueEvent`/`RegisterEventListener` from script) — not applicable, no scripting layer exists

frame-3 has no scripting language integration at all — the build is plain C/C++ (raylib +
`entt`), no Lua (or any other embedded language) is vendored, linked, or referenced anywhere in
the Makefile or `src/`. This piece of the proposal has no target to attach to; adopting it would
mean deciding to add a scripting layer first, which is a separate, much larger decision this ADR
is not making. Revisit only if/when embedding a scripting language is decided on its own merits —
not as a side effect of an event-manager detail pass.

## Decision

Extend `EventManager` (`src/app/event_manager.h`) with `Queue<T>`/`DispatchQueued()` per §3,
ticked once per frame from `Engine::Run()` alongside `ProcessManager::Update()`. Keep `Emit<T>`
unchanged for immediate dispatch. Do not adopt GUID+enum typing (§1), do not add an
`IEventData`/`BaseEventData` mandatory interface (§2 — add a timestamp field to individual event
structs only when a concrete consumer needs one), and do not add Lua integration (§4).

## Tradeoffs accepted

- Two dispatch entry points (`Emit`/`Queue`) instead of one — more API surface than ADR-0002
  shipped, but each has a distinct, real use (true immediate delivery vs. safe-to-cascade
  cross-system delivery); collapsing to one would force every caller into whichever semantics
  don't fit their case.
- `Queue<T>` requires `T` to be copyable into a `std::function` closure (same requirement `Emit<T>`
  already has) — no new constraint introduced.
- No timestamp, ordering guarantee, or dedup logic across events queued in the same frame beyond
  insertion order — matches the book's own guarantee (queue order, not priority order); revisit
  only if a concrete ordering bug appears.
- Still no `Unsubscribe` — out of scope for this ADR (not part of the proposal under review), but
  worth flagging as an existing gap now that `Queue`/re-entrant dispatch makes handler lifetime
  more likely to matter; left as an open question below rather than solved speculatively here.

## Consequences / follow-ups

- Once `Queue<T>`/`DispatchQueued()` land in code, update `.claude/skills/engine-architecture`
  §1 (currently only documents `Subscribe`/`Emit`) and this ADR's Status to Accepted, per the
  precedent in ADR-0002's own "update once it lands" note.
- `Engine::Run()`'s `TickAndUpdateDraw` trampoline (`src/app/engine.cpp`) gains a second per-frame
  call (`Events().DispatchQueued()`) alongside `Processes().Update(...)` — a small, mechanical
  change to an already-shared code path, not a new one.
- First real event type and first real `Subscribe`/`Queue` call sites should also be the first
  test of whether `Queue<T>`'s copy-into-closure approach is efficient enough for
  frequently-fired, larger event payloads — revisit with an rvalue/move-focused API only if that
  turns out to matter.

## Open Questions

- **`Unsubscribe<T>`** — not designed here; no code subscribes yet, so there's no concrete
  lifetime bug to design against. Worth its own pass once a subscriber that can outlive its
  interest in an event type exists (e.g. a UI element removed while still subscribed).
- **Should `Queue<T>` accept move-only event types?** The sketch above copies `T` into the
  closure; a `unique_ptr`-payload event (matching the book's `shared_ptr<IEventData>` closer) is
  possible but adds complexity with no current use case driving it.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 4 — `IEventData`, `BaseEventData`,
  GUID-based `EventType`, `IEventManager::VTriggerEvent`/`VQueueEvent`, the two-alternating-queue
  processing model, and the Lua `ScriptEvent` bridge.
- [ADR-0002](0002-event-manager-and-process-manager-game-loop.md) — the shipped `EventManager`
  (`std::type_index` dispatch, `Emit<T>`, no GUID/enum, no budget) this ADR builds on and
  partially extends.
- `.claude/skills/engine-architecture/SKILL.md` §1 — current `EventManager` documentation, to be
  updated once `Queue`/`DispatchQueued` land.
- `src/app/event_manager.h`, `src/app/engine.h`/`.cpp` — the current implementation and per-frame
  tick site this ADR's §3 addition attaches to.
