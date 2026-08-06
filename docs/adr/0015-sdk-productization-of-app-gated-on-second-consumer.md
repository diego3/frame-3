# 15. SDK productization of `app/`, gated on a second game or an editor existing

- Status: Proposed
- Date: 2026-08-05

## Context

This ADR records an idea from a conversation, not a concrete problem yet: Riot Games has a
"publish platform" team that builds SDKs consumed by their game teams, which prompted the
question of what in frame-3 could become an SDK the same way.

Worth separating two different things Riot's example blends together, since only one of them maps
onto frame-3 today:

- **Publish-platform SDKs** (telemetry, anti-cheat, accounts, storefront, cross-game
  friends/party) are infrastructure *around* a game — each Riot title runs its own engine
  (licensed or in-house); what's shared is the platform glue, not the engine. frame-3 has none of
  this surface at all (no accounts, no telemetry, no storefront), so it isn't what this ADR is
  about.
- **The engine core as an SDK** — `app/` (`Engine`, `EventManager`, `ProcessManager`,
  `ResourceCache<T>`, `EntityFactory`, `BaseGameLogic`/`IGameView`) already has the shape of one:
  [ADR-0014](0014-game-module-boundary-and-template-migration.md) formalizes exactly the boundary
  an SDK would need — `app/` game-agnostic, `game/<game-id>/` extending it, never the reverse. A
  tools/editor binary built on the same `Engine`/`EntityFactory`/`ResourceCache` types is a
  plausible future consumer of that same boundary. This is the half worth recording.

The architecture direction is already correct and doesn't need re-deciding — it's what ADR-0014
does. What "becoming an SDK" would add on top is a set of obligations this project has no reason
to take on yet: a versioned public API surface, compatibility guarantees, documentation aimed at
an external consumer instead of a contributor reading the source, and a packaging/distribution
story so a new `game/<game-id>/` tree (or an editor binary) can depend on `app/` without living in
the same checkout. None of that can be designed correctly right now, because there's only one
consumer:

- `src/game/` is still mid-migration off the raylib template's C screens (ADR-0014's own scope);
  the boundary's exact shape is still moving.
- The ECS has one real component (`LocalTransform`/`WorldTransform`) and one real content pair
  (`assets/levels/level_01.yaml`, `assets/entities/player.yaml`) — not enough surface to know
  which parts of `app/` a second, differently-shaped game would actually need frozen.
- No editor exists. Editors are normally built once there's enough real content and gameplay to
  need editing, not ahead of it.

An API frozen from a single consumer's usage is a guess, not a validated boundary — the second
consumer is what tells you where the abstraction actually leaks.

## What "becoming an SDK" would concretely add (naming the shape now, not deciding it yet)

- A versioned public API surface for `app/`'s headers, with an explicit compatibility policy
  (semver or equivalent) — currently every `app/` change is free to break `game/` in the same
  commit, which is correct for a single-consumer repo and wrong for an SDK.
- Documentation written for an external consumer (what to subclass, what's stable, what's
  internal) rather than the current contributor-facing comments and ADRs.
- A packaging/distribution mechanism — e.g., `app/` consumable as a library/submodule by a
  `game/<game-id>/` tree that isn't this same checkout — instead of today's single-repo layout.
- A concrete extension-point contract for a new game: how it registers `EntityFactory` component
  loaders, its `BaseGameLogic`/`IGameView` subclasses, and its own screens. ADR-0014's migration
  work will answer parts of this as a side effect; worth revisiting this ADR once that lands to see
  how much is already settled.

## Decision

**Not adopted now.** Deferred, same "gated on a concrete trigger" pattern as
[ADR-0007](0007-terraform-gated-on-authoritative-server.md). The trigger: a second real consumer
of `app/` actually being built — either a second game, or an editor/tooling binary built directly
on `Engine`/`EntityFactory`/`ResourceCache` — not a hypothetical one. At that point, design the
versioning, docs, and packaging shape from what that second consumer actually needed, rather than
guessing ahead of time from a single data point.

## Consequences / follow-ups

- No code or architecture changes from this ADR — it exists so the idea survives until the trigger
  is met, not to ship anything.
- [ADR-0014](0014-game-module-boundary-and-template-migration.md)'s `app/`/`game/` boundary and
  C→C++ migration should land on its own merits regardless of this ADR — it's a prerequisite this
  ADR would build on, not a reason to build it.
- When the trigger is met, write the real ADR for versioning/packaging/docs, and link back here for
  the original reasoning.
- **Trigger met, narrowly** (2026-08-05): [ADR-0017](0017-camera-fps-second-game-module.md) added
  `game/camera_fps`, a second real consumer of `app/`. What it concretely needed generalized turned
  out to be small and mechanical — the `IScreenElement` stack plumbing, promoted into a new
  `app/human_view_base.h`/`.cpp` both game modules now subclass — not evidence that the broader
  productization this ADR describes (versioned API, packaging, external-consumer docs) is
  warranted yet. Status stays `Proposed`: this ADR's actual decision (defer that broader work)
  hasn't changed, only the narrow open question it flagged about a generic `HumanView` base has
  been answered.

## References

- [ADR-0014](0014-game-module-boundary-and-template-migration.md) — the `app/` vs `game/`
  dependency direction this would build on if adopted.
- [ADR-0007](0007-terraform-gated-on-authoritative-server.md) — precedent for recording a deferred
  decision gated on a concrete future trigger instead of building ahead of need.
