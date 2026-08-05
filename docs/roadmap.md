# frame-3 roadmap: *Game Coding Complete* coverage

A checklist, not a decision record — `docs/adr/` is where the *why* for each of these lives; this
file is just the fast-scan index of what's landed, what's decided-but-not-built, and what's not
been looked at yet. Check an item off only once its code has actually shipped (not when an ADR
proposing it is merged) — a `Status: Proposed` ADR gets a link here, not a checkmark. When an item
does land, flip its box here **and** flip that ADR's own `Status:` field to `Accepted` in the same
change, so this file and `docs/adr/` never disagree about what's actually built.

Grouped by where each item currently stands, not by book chapter order — see each linked ADR for
the chapter reference and full reasoning.

## Shipped

- [x] Event Manager, typed dispatch (`Subscribe`/`Emit`) — [ADR-0003](adr/0003-event-manager-and-process-manager-game-loop.md)
- [x] Process Manager — [ADR-0003](adr/0003-event-manager-and-process-manager-game-loop.md)
- [x] Application layer / main loop (`Engine`) — [ADR-0001](adr/0001-ecs-via-entt-and-cpp-engine-init.md) Decision 2
- [x] Actors/Components → ECS via EnTT — [ADR-0001](adr/0001-ecs-via-entt-and-cpp-engine-init.md) Decision 1
- [x] Resource cache (`ResourceCache<T>`: Font/Sound/Model/Texture2D/Shader) — [ADR-0004](adr/0004-resource-cache-thin-vs-full-book-rescache.md)
- [x] Unit test framework (doctest) — [ADR-0006](adr/0006-doctest-for-unit-tests.md)
- [x] Queued event dispatch (`Queue`/`DispatchQueued`) — [ADR-0005](adr/0005-event-manager-queued-dispatch-idata-lua-proposal.md) §5

## Decided (ADR merged), not yet built

- [ ] Scene graph / transform hierarchy — [ADR-0002](adr/0002-scene-graph-hierarchy-options.md)

## Proposed (ADR written, PR open, not yet merged)

- [ ] Data-driven entity/component loading (YAML) — [ADR-0008](adr/0008-data-driven-entity-loading-yaml.md) — PR #12
- [ ] Level loading (`LevelLoader`, actor placement + overrides) — [ADR-0009](adr/0009-level-loading-actor-placement.md) — PR #13, depends on #12
- [ ] Event serialization for networking + save/replay journal (`ISerializableEvent`, `EventJournal`) — [ADR-0005](adr/0005-event-manager-queued-dispatch-idata-lua-proposal.md) §§1-4

## Not started — no ADR yet

Ordered by rough impact, not book chapter order. Each of these needs its own ADR before landing
any code, same as everything above did.

- [ ] **Physics / collision** — nothing exists yet: no ADR, no skill, no library choice made. The
  book dedicates a full chapter to this; biggest unaddressed gap in the project.
- [ ] **`BaseGameLogic`/`IGameView` split** (HumanView/RemoteView/AIView) — deliberately deferred
  since [ADR-0001](adr/0001-ecs-via-entt-and-cpp-engine-init.md) Decision 2, reaffirmed in
  [ADR-0009](adr/0009-level-loading-actor-placement.md). Unlocks AI reacting to world events and
  network replication (both listed below) once it exists.
- [ ] **AI** (FSM, utility scoring, steering, perception, pathfinding) — design guidance already
  exists in `.claude/skills/engine-ai-behavior`, but zero code and zero entities to apply it to.
- [ ] **Network transport** (sockets/library choice, client/server architecture) — the event-level
  contract is proposed in ADR-0005 §§1-4 above; the actual wire transport is explicitly left for a
  future ADR "once multiplayer work actually starts."
- [ ] **Scripting (Lua)** — explicitly rejected for now in
  [ADR-0005](adr/0005-event-manager-queued-dispatch-idata-lua-proposal.md) §7 ("no scripting layer
  exists to attach it to"); revisit only if embedding a scripting language is decided on its own
  merits.
- [ ] **Game options/config file** (resolution, controls, etc.) — not discussed anywhere yet.
- [ ] **Save/load of actual game state** — distinct from ADR-0005's event journal (which persists
  *events*, not a snapshot of world state); not designed.
- [ ] **UI/HUD as a system** (the book's `IScreenElement` stack) — what exists today is just the
  raylib template's logo/title/gameplay/ending/options screen state machine, not a real UI system.
- [ ] **Custom memory manager** — the book has a dedicated chapter; not even discussed here, likely
  correctly deferred (standard allocators are fine until profiling says otherwise) but never
  formally decided.
- [ ] **Multithreading / job system** — not discussed.

## How to use this file

1. Picking up an item from "Not started"? Write its ADR first (same process as every item above
   went through), then implement once that ADR is accepted.
2. Landing an item from "Decided" or "Proposed"? Flip its box to `[x]`, move it up to "Shipped",
   and flip the ADR's `Status:` field to `Accepted` in the same PR.
3. This file drifting out of sync with `docs/adr/`'s actual statuses is a bug in this file — fix it
   on sight rather than letting both drift.
