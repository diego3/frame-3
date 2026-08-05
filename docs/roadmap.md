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
- [x] Scene graph / transform hierarchy (`Relationship`, `LocalTransform`/`WorldTransform`, `PropagateTransforms`) — [ADR-0002](adr/0002-scene-graph-hierarchy-options.md)
- [x] Event serialization contract (`ISerializableEvent`, `EventTypeRegistry`, FNV-1a stable type ID) — [ADR-0005](adr/0005-event-manager-queued-dispatch-idata-lua-proposal.md) §§1-3
- [x] Data-driven entity/component loading (`EntityDefNode`, `IEntityFileParser`/`YamlEntityFileParser`, `EntityFactory`) — [ADR-0008](adr/0008-data-driven-entity-loading-yaml.md)

## Decided (ADR merged), not yet built

_(nothing currently — the last item here, scene graph/hierarchy, shipped above)_

## Proposed (ADR merged into `main`, `Status: Proposed` — design not yet built)

- [ ] Level loading (`LevelLoader`, actor placement + overrides) — [ADR-0009](adr/0009-level-loading-actor-placement.md), depends on ADR-0008 (now shipped above) — nothing in the codebase constructs a real `EntityFactory`/registers a real `ComponentLoader` yet, that's this item's job
- [ ] Event journal for save/replay (`EventJournal`) — [ADR-0005](adr/0005-event-manager-queued-dispatch-idata-lua-proposal.md) §4 — the serialization contract it would sit on (§§1-3) already shipped above; no concrete on-disk format decided yet
- [ ] `BaseGameLogic`/`IGameView` split (`HumanView` built; `RemoteView`/`AIView` named, not built) — [ADR-0010](adr/0010-base-game-logic-and-igameview.md), depends on ADR-0008/0009 landing first

## Not started — no ADR yet

Ordered by rough impact, not book chapter order. Each of these needs its own ADR before landing
any code, same as everything above did.

- [ ] **Physics / collision** — nothing exists yet: no ADR, no skill, no library choice made. The
  book dedicates a full chapter to this; biggest unaddressed gap in the project.
- [ ] **AI** (FSM, utility scoring, steering, perception, pathfinding) — design guidance already
  exists in `.claude/skills/engine-ai-behavior`, but zero code and zero entities to apply it to.
  Unlocked by `AIView` once [ADR-0010](adr/0010-base-game-logic-and-igameview.md) is implemented.
- [ ] **Input / key-binding system** (data-driven action↔key/gamepad-button mapping, rebindable by
  the player) — not designed anywhere yet. [ADR-0010](adr/0010-base-game-logic-and-igameview.md)
  decided `HumanView` polls raylib input directly (`IsKeyDown`/etc.) rather than a Win32-style
  message-proc layer, but explicitly left "which key/gesture drives which actor action" as an open
  question (§ Open Questions) — that's this item. Distinct from the config-file bullet below
  (resolution/audio settings vs. actual action mapping + a rebinding UI + persisting the player's
  choice).
- [ ] **Network transport** (sockets/library choice, client/server architecture) — the event-level
  contract (`ISerializableEvent`/`EventTypeRegistry`, ADR-0005 §§1-3) already shipped above; the
  actual wire transport is explicitly left for a future ADR "once multiplayer work actually
  starts."
- [ ] **Scripting (Lua)** — explicitly rejected for now in
  [ADR-0005](adr/0005-event-manager-queued-dispatch-idata-lua-proposal.md) §7 ("no scripting layer
  exists to attach it to"); revisit only if embedding a scripting language is decided on its own
  merits.
- [ ] **Game options/config file** (resolution, audio volume, window mode, etc.) — not discussed
  anywhere yet. Distinct from the input/key-binding item above (display/audio settings, not action
  mapping).
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
