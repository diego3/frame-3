# 9. Level loading: actor placement over a level file, without a `BaseGameLogic` yet

- Status: Proposed
- Date: 2026-08-04

## Context

[ADR-0008](0008-data-driven-entity-loading-yaml.md) designed loading *one entity's* component
makeup from a data file (`EntityFactory` + `EntityDefNode` + `IEntityFileParser`). *Game Coding
Complete* Ch. 9-10 (`BaseGameLogic::VLoadGame`) covers a related but distinct concern: a **level**
file — a list of *placements* of those entity definitions (which one, where, with what per-instance
overrides), plus level-wide settings — loaded all at once when a level starts.

In the book, `VLoadGame` opens a level resource (e.g. `world.xml`), walks its `<Actor>` elements,
and for each one calls into the `ActorFactory` (ADR-0008's territory) with a resource reference
plus a placement transform and optional per-instance overrides, adds the result to
`BaseGameLogic`'s own actor map, and fires `EvtData_New_Actor` so the View layer can react (e.g.
attach a render-side Scene Node). `BaseGameLogic` itself is the book's Game Logic layer object
(Ch. 9-10) — a class this project doesn't have, and
[ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) Decision 2 explicitly deferred deciding
whether/how to split Game Logic from Game View at all, rather than guessing at it ahead of a
concrete need.

This ADR was prompted by two related questions: how should level loading work given what ADR-0008
already built, and — separately — is *this* the point where a `BaseGameLogic`-equivalent needs to
exist. Answered together below, because the second question's answer shapes where the first
question's code is allowed to live.

## Is it time for a `BaseGameLogic`? Not yet — level loading doesn't need one.

Three things `BaseGameLogic` bundles in the book already exist here as separate systems, not as
one monolithic Logic object waiting to be built:

- **The actor map** (`m_actors`) — this project's `entt::registry` (owned by `Engine`, ADR-0001)
  already *is* that map; there's no separate collection to invent.
- **Process management** — already `ProcessManager` (ADR-0003), owned by `Engine`, ticked every
  frame.
- **Level loading itself** — the thing this ADR designs — doesn't need a `BaseGameLogic` host
  object to exist in; it needs a `entt::registry&` (already available via `Engine::Registry()`)
  and the pieces ADR-0008 already built. A plain `LevelLoader` class, constructed with references
  to what it needs, does the job without a Logic/View split behind it.

What `BaseGameLogic` *would* still add beyond what's already built: a top-level game-state machine
(loading/running/paused) and, per the book's own design, the actual seam between "the simulation
changed" and "how it's drawn" — which is exactly the split ADR-0001 Decision 2 deferred, for a
stated reason (`screens.h`'s five `screen_*.c` files are still fused Update+Draw per screen;
committing to a Logic/View boundary now would mean guessing at how that fusion eventually splits).
Nothing about loading a level changes that calculus — a level loader that populates the registry
doesn't need to know whether the code calling it is "Logic" or "View" in some future split.

**Decided here: no `BaseGameLogic` yet.** The concrete trigger to revisit: once the fused
Update+Draw model in `screens.h` visibly can't keep growing without duplicating logic across
screens, or — more concretely, given ADR-0005's networking proposal — once multiplayer needs a
server loop that runs simulation with no drawing at all. Either is a real, felt need; guessing at
the shape now, just because a level loader happens to sit near where `BaseGameLogic::VLoadGame`
lives in the book, would be deciding a bigger question opportunistically instead of on its own
merits. Recorded as an Open Question below so it stays visible rather than only living in this
paragraph.

## Decision — `LevelLoader`, composing ADR-0008's pieces plus placement + overrides

### Level file: a list of actor placements, referencing ADR-0008's entity files

```yaml
# assets/levels/level_01.yaml
actors:
  - resource: entities/enemy_slime.yaml
    position: { x: 10, y: 0, z: 5 }
  - resource: entities/enemy_slime.yaml
    position: { x: 15, y: 0, z: 5 }
    overrides:
      Health: { max: 25 }   # this particular slime spawns weaker than enemy_slime.yaml's default
  - resource: entities/player_start.yaml
    position: { x: 0, y: 0, z: 0 }
settings:
  camera:
    position: { x: 0, y: 5, z: -10 }
```

Parsed with the exact same `IEntityFileParser`/`EntityDefNode` from ADR-0008 — a level file is
just another YAML document; nothing about it needs a second parser or a second value-tree type.
`position` is sugar for an override targeting the `Position` component specifically (the
overwhelmingly common per-instance override), translated into the same mechanism `overrides`
uses below, not a separate code path.

### Merging per-instance overrides — shallow, on purpose

```cpp
// entity_def.h addition (sketch)
// Shallow merge: for each top-level key (component name) present in `overrides`, that whole
// component's data node replaces the one in `base`; components not mentioned in `overrides` are
// copied from `base` unchanged. Does not deep-merge individual fields *within* one component (an
// override replaces a whole component's node, not one field inside it) -- simpler than a
// recursive field-level merge, and enough for "this instance has less health" without needing
// partial-field merge semantics yet. Revisit only if a real case needs one field overridden while
// keeping the rest of that same component's base values.
EntityDefNode MergeOverrides(const EntityDefNode &base, const EntityDefNode &overrides);
```

### `LevelLoader`

```cpp
// level_loader.h/.cpp (sketch)
class LevelLoader {
public:
    LevelLoader(EntityFactory &entityFactory, IEntityFileParser &parser);

    // Parses `levelPath`; for each `actors[]` entry, parses the referenced entity resource file
    // (ADR-0008), merges `position`/`overrides` on top via MergeOverrides(), creates the entity
    // through `entityFactory_`, and fires EvtData_EntitySpawned for it. Returns every entity
    // created, in file order.
    std::vector<entt::entity> Load(entt::registry &registry, EventManager &events,
                                    const std::string &levelPath);

private:
    EntityFactory &entityFactory_;
    IEntityFileParser &parser_;
};
```

`LevelLoader` doesn't own an `entt::registry` or an `EventManager` — both are passed in per call,
the same way `EntityFactory::Create` already takes a `registry` argument rather than owning one
(ADR-0008). Consistent with "no `BaseGameLogic` yet" above: `LevelLoader` is a plain utility class,
constructible and callable from wherever gameplay code currently lives (today: a small
`extern "C"` bridge function callable from `screen_gameplay.c`'s `InitGameplayScreen()`, the same
kind of bridge already needed anywhere `screens.h`'s plain-C files reach into this project's C++
systems — not a new problem this ADR introduces).

### Why `LevelLoader` fires `EvtData_EntitySpawned` unconditionally — the View-plurality seam, kept on purpose

Earlier framing of this ADR (before review) called this event optional, on the reasoning that a
pull-based renderer (`registry.view<Renderable, ...>()`, run every frame) never needs to be told a
new entity exists — it just finds it on the next query. That's true, but it's the wrong lens for
*why* the book fires this event at all, and undersells what Ch. 9's View abstraction is actually
for.

`BaseGameLogic` in the book doesn't know or care how many `IGameView`s are attached, or what kind:
a local `HumanView` (renders), a `RemoteView` (a proxy that forwards world changes to an actual
networked client — no local rendering at all), an `AIView` (a bot's own decision-making state, also
no rendering). Logic never special-cases any of them — it just fires events (`EvtData_New_Actor`,
`EvtData_Destroy_Actor`, ...) and stays completely ignorant of who's listening or how many. That
decoupling is the actual payoff, and it doesn't reduce to "how does rendering find out":

- **A `RemoteView` cannot pull.** It's not in this process — there is no shared registry for a
  networked client to query. The *only* way to tell it a new entity exists is to push something
  across the network, which is exactly the serialization path already designed in
  [ADR-0005](0005-event-manager-queued-dispatch-idata-lua-proposal.md) (`ISerializableEvent`,
  `EventTypeRegistry`) — this ADR's `EvtData_EntitySpawned` is the natural first real event that
  design would carry once a `RemoteView`-equivalent subscribes and forwards it.
- **An `AIView`-style bot could technically pull** (it's in-process, same as the renderer) — but
  reacting to "something happened" as a discrete event is how the book's AI subsystems
  (`.claude/skills/engine-ai-behavior`, Ch. 11-13) are meant to key off world changes, not by
  diffing the full registry every tick to reconstruct what changed since last frame.

**Decided: fire `EvtData_EntitySpawned` from `LevelLoader::Load` unconditionally, even though no
`RemoteView`/`AIView`-equivalent subscriber exists yet.** This is a deliberate exception to this
project's usual "don't build ahead of need" discipline (ADR-0001's ECS choice, ADR-0004's thin
cache, ADR-0008's mini-yaml pick) — and the exception is principled, not just "the book does it so
we will too": the whole value of firing this event is that `LevelLoader` (and whatever Logic-side
code eventually calls it) never has to change when a second kind of view gets added later. That
property only holds if the event is fired from the start; unlike LRU eviction or a pluggable
resource loader (things genuinely retrofittable later with no cost to waiting), *this* seam is the
one place where waiting for "a concrete need" would mean touching `LevelLoader` again the day a
`RemoteView` or `AIView` actually gets built — defeating the point of decoupling Logic from View in
the first place. Local rendering keeps using pull regardless (it's simpler, and correct, for a
same-process renderer reading the same registry things were created in) — this isn't "replace pull
with push," it's "keep pushing lifecycle events *in addition to* whatever pulls, because pull only
ever covers same-process consumers."

## Tradeoffs accepted

- Shallow-merge-only overrides (whole component replaced, not field-level) — accepted for the same
  reason ADR-0008 accepted mini-yaml's partial spec coverage: covers the actual need (tune a
  handful of values per instance) without building a general recursive merge nobody's asked for
  yet.
- No `BaseGameLogic`/Logic-View split — this ADR explicitly defers that (see above), consistent
  with ADR-0001 Decision 2's original deferral; `LevelLoader` is deliberately unopinionated about
  which future layer calls it.
- `EvtData_EntitySpawned` has no subscriber yet (no `RemoteView`/`AIView`-equivalent exists) —
  accepted *deliberately*, unlike this project's usual defer-until-needed discipline: see "Why
  `LevelLoader` fires... unconditionally" above for why this specific seam doesn't get the same
  treatment as, say, LRU eviction or a pluggable loader registry.
- No unloading/level-transition story (despawning the previous level's entities before loading a
  new one) — not designed here; `LevelLoader::Load` only adds entities, it doesn't know how to
  tear a level back down. Revisit once a second level actually needs loading.
- No streaming/partial loading (large open-world-style incremental level loading) — not a concern
  at this project's current scale; `Load` parses and instantiates everything in one pass.

## Consequences / follow-ups

- `.claude/skills/engine-architecture` should gain a short note once `LevelLoader`/
  `MergeOverrides`/`EvtData_EntitySpawned` land in code, alongside ADR-0008's own follow-up note.
- The `extern "C"` bridge needed to call `LevelLoader` from `screen_gameplay.c` is the first real
  instance of a C screen file reaching into ADR-0008/ADR-0009's C++ systems directly (not just
  through `Engine`'s existing extern globals) — worth checking whether that bridge shape stays
  comfortable once more than one screen needs to trigger a level load.
- Once `EventManager::Queue`/`DispatchQueued` (ADR-0005) actually lands, `LevelLoader` firing
  `EvtData_EntitySpawned` should switch from `Emit` to `Queue` — mechanical, not a design change.
- The "is it time for `BaseGameLogic`" question is answered "not yet" here, but explicitly not
  closed — see Open Questions.
- Whatever eventually builds a `RemoteView`-equivalent should reach for
  `EvtData_EntitySpawned` + ADR-0005's `ISerializableEvent`/`EventTypeRegistry` as the forwarding
  path, rather than inventing a separate network-replication mechanism — that pairing is exactly
  what both ADRs exist to hand it.
- **Symmetry gap, not designed here**: this ADR only covers entities appearing (`LevelLoader`
  creates, never destroys). Whatever eventually despawns entities will need an
  `EvtData_EntityDestroyed` counterpart for the same reason `EvtData_EntitySpawned` exists — a
  `RemoteView` needs to know when to stop replicating something, an `AIView` needs to forget a
  dead target. Flagged so it isn't rediscovered as a gap only once something actually destroys an
  entity.

## Open Questions

- **When does the Logic/View split (and a `BaseGameLogic`-shaped object) actually become worth
  building?** Named two concrete triggers above (screens.h's fused model straining, or a
  multiplayer server loop needing simulation-without-drawing per ADR-0005) — neither has happened
  yet. Worth its own ADR when either does, rather than retrofitting this one.
- **Level transitions/unloading** — not designed here (see Tradeoffs); needs an answer once a
  second level exists to transition to.
- **Field-level override merging** — deferred (see Tradeoffs); revisit if a real case needs
  partial-component overrides.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 9-10 — `BaseGameLogic::VLoadGame`,
  `EvtData_New_Actor`, the Actor map, and the Logic/View split this ADR keeps deferred.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) Decision 2 — the original Logic/View
  deferral this ADR reaffirms rather than revisits.
- [ADR-0003](0003-event-manager-and-process-manager-game-loop.md) — `EventManager::Emit`, used
  here for `EvtData_EntitySpawned`.
- [ADR-0005](0005-event-manager-queued-dispatch-idata-lua-proposal.md) — `Queue`/`DispatchQueued`
  (not yet built) and the networking proposal whose server-loop need is one of the two named
  triggers for revisiting the Logic/View split.
- [ADR-0008](0008-data-driven-entity-loading-yaml.md) — `EntityFactory`, `EntityDefNode`,
  `IEntityFileParser`, all reused directly here rather than duplicated.
