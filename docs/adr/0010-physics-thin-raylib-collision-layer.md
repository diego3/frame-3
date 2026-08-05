# 10. Physics: thin raylib-collision layer behind an `IGamePhysics`-shaped seam, real dynamics deferred

- Status: Proposed
- Date: 2026-08-04

## Context

Nothing in this project addresses physics or collision in any form — no ADR, no skill, no library
choice, not even a mention. Every other Game Coding Complete-derived system in frame-3 has at least
been evaluated (event/process manager, resources, entity loading, level loading); physics hasn't,
and [`docs/roadmap.md`](../roadmap.md) flags it as the highest-impact remaining gap. This ADR is
that evaluation.

*Game Coding Complete*'s physics chapter builds an `IGamePhysics` interface with two
implementations: `NullPhysics` (a no-op stub — movement and collision are simply not simulated) and
a real implementation wrapping a third-party rigid-body engine (the book uses Bullet). A
`PhysicsComponent` (Ch. 6-7's Actor/Component pattern) holds an actor's physics data (shape,
density, material) and, on init, calls into `IGamePhysics` to create the actual rigid body;
collision events (`EvtData_PhysCollision`/`EvtData_PhysSeparation`) fire through the Event Manager
(Ch. 4) so gameplay code reacts to collisions without polling. The interface exists specifically so
a simple game ships with `NullPhysics` and a more ambitious one swaps in the real engine without
gameplay code (which only ever calls `IGamePhysics::VApplyForce(actorId, ...)` etc.) changing.

Two things need re-deciding for this project specifically, the same way every prior
book-architecture ADR has: how "PhysicsComponent" maps onto ECS instead of an Actor hierarchy
(ADR-0001), and whether to adopt a real third-party rigid-body engine now or build a smaller thing
first — the same "does raylib already do this, and does the candidate library fit this repo's
Makefile-only build" lens ADR-0004 (resources) and ADR-0008 (YAML) already applied.

## Decision — collision detection only, via raylib's own primitives, behind a kept `IGamePhysics` seam

### raylib already does real (if basic) 3D collision detection

Unlike a texture/model loader (ADR-0004) or a config parser (ADR-0008), raylib doesn't ship a
rigid-body dynamics engine — but it does ship real collision-detection primitives, as part of core
`raylib.h`, not a separate module: `CheckCollisionBoxes`, `CheckCollisionSpheres`,
`CheckCollisionBoxSphere`, `CheckCollisionPointX` variants, and `GetRayCollisionSphere`/`Box`/
`Mesh`/`Triangle`/`Quad` for raycasts. No dynamics (no forces, mass, velocity integration, or
constraint solving) — just "do these two shapes overlap right now," which is exactly what a
collision-*detection* layer needs and nothing a full engine would add on top for that specific
question.

### ECS mapping: colliders are plain components, physics is a system, not an Actor method

```cpp
// physics/collider.h (sketch)
struct BoxCollider { Vector3 halfExtents; };
struct SphereCollider { float radius; };
// Paired with whatever position/transform component already exists on the entity (ADR-0008's
// EntityFactory sketches used a Position component) -- a collider doesn't carry its own position.
```

Same re-interpretation ADR-0001 §3 already applied to `SpawnEnemy`-style factories: the book's
`PhysicsComponent::VPostInit()` calling into `IGamePhysics` to create a body becomes, here, just
emplacing a plain `BoxCollider`/`SphereCollider` onto an entity — no virtual method, no physics
object to construct, the data *is* the component.

### `IGamePhysics` — kept, because the seam is the actual point, not the engine behind it

```cpp
// physics/game_physics.h (sketch)
class IGamePhysics {
public:
    virtual ~IGamePhysics() = default;

    // Checks every pair of colliding-enabled entities this frame, firing EvtData_CollisionBegin/
    // EvtData_CollisionEnd (via `events`) for pairs whose overlap state changed since the last
    // call. Not a dynamics step -- no forces, mass, or velocity integration happen here.
    virtual void Update(entt::registry &registry, EventManager &events, float dt) = 0;
};

class RaylibCollisionPhysics : public IGamePhysics {
public:
    void Update(entt::registry &registry, EventManager &events, float dt) override;

private:
    // raylib's CheckCollisionX calls are stateless per call -- they don't know about "last frame"
    // on their own. This is what lets Update() tell "started overlapping this frame" (fire Begin)
    // apart from "still overlapping" (fire nothing) and "stopped overlapping" (fire End).
    std::set<std::pair<entt::entity, entt::entity>> overlappingLastFrame_;
};
```

`IGamePhysics` is kept deliberately, even though only one implementation exists — the same
reasoning ADR-0004 used for pulling `ResHandle`'s `shared_ptr` piece forward and ADR-0008 used for
`IEntityFileParser`: the seam is what lets gameplay code call `physics.Update(...)` (or, once real
dynamics exist, `physics.ApplyForce(...)`) without caring which engine is under it. Unlike those
two cases, this seam is being kept from day one specifically *because* the eventual upgrade (real
rigid-body dynamics) is foreseeable and non-trivial enough that discovering the seam should have
existed only after the fact would mean rewriting every call site, not just swapping an
implementation.

### Collision events fire unconditionally — same reasoning as ADR-0009's `EvtData_EntitySpawned`

`RaylibCollisionPhysics::Update` fires `EvtData_CollisionBegin`/`EvtData_CollisionEnd` through
`EventManager` regardless of whether a subscriber exists yet, for the same reason
[ADR-0009](0009-level-loading-actor-placement.md) decided `EvtData_EntitySpawned` isn't optional: a
sound system reacting to "something just collided," an `AIView`-equivalent updating threat
awareness, or a future `RemoteView` replicating the collision to a client all need a *push*
notification at the moment it happens — none of them can be implemented as "poll the registry every
frame and diff," the way a pull-based renderer can.

### Where this lives: not owned by `Engine`, same as `LevelLoader`

Physics simulation is a Game Logic/simulation concern, not an Application-layer one — `Engine`
deliberately doesn't own Game Logic+View concerns (ADR-0001 Decision 2). `IGamePhysics`/
`RaylibCollisionPhysics` is a plain object constructed and ticked by whatever gameplay code exists
today (the fused `screen_gameplay.c`, via a small `extern "C"` bridge — the same bridging shape
`LevelLoader` already needs, per ADR-0009), not an `Engine` member.

## Options considered for adopting a real rigid-body engine now instead

| | **raylib collision only** (recommended) | **Jolt Physics** | **Bullet** (book's own pick) | **Hand-rolled dynamics** |
|---|---|---|---|---|
| Vendoring shape | None — already in raylib, already a dependency. | CMake-based build. | CMake-based build (legacy Makefiles exist but aren't the maintained path). | None — no new dependency. |
| Fits Makefile-only build (ADR-0001 Decision 3, reaffirmed ADR-0006) | Yes, trivially. | **No**, as-is — see note below. | **No**, as-is — see note below. | Yes. |
| Capability | Discrete overlap checks + raycasts only — no forces, mass, momentum, joints, continuous collision detection. | Full rigid-body dynamics, modern C++17/20, MIT, actively maintained, used in shipped AAA titles (e.g. Horizon Forbidden West) — genuinely the strongest modern option on technical merit. | Full rigid-body dynamics, the book's own choice, mature, zlib license, but an older (C++03-era) codebase than Jolt. | Whatever we implement — a real from-scratch dynamics/constraint solver is a substantial, error-prone undertaking (this is precisely the kind of problem serious engines spend years on). |
| Matches this project's current need | Yes — no gameplay exists yet; likely first real needs are "did the player's capsule hit a wall," "did a projectile hit a hitbox," not realistic momentum transfer. | Overkill for a project with zero entities today. | Overkill, same as Jolt, plus a less modern codebase. | Rejected — the "build it ourselves" call that worked for the scene graph (ADR-0002, a genuinely small, well-scoped problem) does not scale to rigid-body dynamics, which is not a small problem. |

**Note on the Makefile-only constraint and physics specifically**: unlike the YAML case
(ADR-0008), where a genuinely simpler, equally-fit-for-purpose header-only alternative
(mini-yaml) existed, there is no realistic "mini-yaml of rigid-body physics" — every serious
option is CMake-based, because the problem itself (broad-phase, narrow-phase, SIMD, constraint
solving) is bigger than what a header-only library reasonably covers. When real dynamics is
actually needed, this project will have to either (a) invoke the chosen engine's own CMake as a
one-time `build.sh` preprocessing step producing a `.a` to link normally (the same *shape* raylib's
own Makefile-invoked-from-`build.sh` already uses, but a CMake invocation specifically — which
ADR-0006 previously avoided even as a one-time step, by picking doctest over GoogleTest instead),
or (b) hand-write a Makefile rule compiling the engine's sources directly (possible in principle —
compiling all of a library's `.cpp` files with consistent flags doesn't strictly require its own
build system — but riskier for a codebase whose CMake does meaningful platform/SIMD/precision
feature detection that a naive compile-everything rule could silently get wrong). Recorded now so
this cost is visible before it's paid, not discovered by surprise in a future ADR.

## Tradeoffs accepted

- No dynamics: no gravity-as-a-force, no momentum/velocity integration, no realistic collision
  response, no continuous collision detection (a fast-moving object can tunnel through a thin
  collider within one frame) — accepted because no gameplay exists yet to need any of this;
  revisit once a concrete case (a projectile that needs to reliably hit a thin wall, physically
  plausible knockback, ragdolls, vehicle physics) actually shows up.
- Only box/sphere collider shapes — raylib's own collision functions don't cover arbitrary convex
  hulls or capsules; accepted since most gameplay starts with primitive collider shapes anyway.
- Pairwise (`O(n²)`) overlap checking, no broad-phase spatial partitioning (grid/BVH) — accepted at
  today's near-zero entity count; the frame-budget SLO (`.claude/skills/engine-sre`) is the
  existing, already-tracked signal for when this stops being fine.
- Movement/position integration is explicitly **not** `IGamePhysics`'s job here (unlike the book's
  real-Bullet-backed implementation, which does own velocity/force integration) — this ADR is
  collision detection only; wherever gameplay code currently moves entities keeps doing so.
- The Makefile-only constraint likely doesn't survive contact with a real physics engine adoption
  later — flagged above rather than assumed away.

## Consequences / follow-ups

- `.claude/skills/engine-architecture` should gain a physics section once
  `IGamePhysics`/`RaylibCollisionPhysics`/`Collider` components land in code.
- `docs/roadmap.md`: move "Physics / collision" from "Not started" to "Proposed" pointing at this
  ADR once merged; move it to "Shipped" once the code lands, per that file's own stated convention.
- The first real gameplay entity with a collider is also the first test of whether
  `EvtData_CollisionBegin`/`End`'s pairwise-entity shape is the right granularity, or whether a
  higher-level "hit" concept (with damage/impulse data attached) belongs on top of it.
- When real dynamics is actually needed: Jolt is the recommended starting point over Bullet on
  technical merit (modern C++, active maintenance) — but the CMake-vendoring problem named above
  needs its own resolution first, as its own ADR, not assumed away by this one.

## Open Questions

- **Should collider shapes be extended (capsule, convex hull) before real dynamics arrives**, using
  more of raylib's own primitives (e.g. `GetRayCollisionMesh` for arbitrary meshes), or should that
  wait for a real engine? Not decided; likely depends on what the first few real entities actually
  need.
- **How does this interact with the still-undecided scene graph** ([ADR-0002](0002-scene-graph-hierarchy-options.md),
  status Proposed)? A collider attached to a child entity in a future transform hierarchy would
  need world-space position resolved before a collision check, not just its local transform — not
  addressed here, worth revisiting once ADR-0002 actually lands.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham) — `IGamePhysics`, `NullPhysics`,
  `PhysicsComponent`, `EvtData_PhysCollision`/`EvtData_PhysSeparation`, the Bullet-backed
  implementation.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) — ECS via EnTT (Decision 1, the
  Actor→component re-mapping this ADR follows) and `Engine`'s Ch. 5-only scope (Decision 2, why
  physics isn't an `Engine` member).
- [ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) — "raylib already does the hard
  part" reasoning, reapplied here to collision detection; the kept-seam-even-when-thin precedent.
- [ADR-0006](0006-doctest-for-unit-tests.md) — the Makefile-only vendoring constraint (no CMake)
  this ADR's engine comparison reapplies, and the precedent for rejecting a CMake-only dependency
  even as a one-time `build.sh` step.
- [ADR-0008](0008-data-driven-entity-loading-yaml.md) — the format-swappable-seam reasoning reused
  here for `IGamePhysics`.
- [ADR-0009](0009-level-loading-actor-placement.md) — the "fire lifecycle events unconditionally,
  even with no subscriber yet" reasoning reused here for `EvtData_CollisionBegin`/`End`.
- [`docs/roadmap.md`](../roadmap.md) — flagged this as the top unaddressed gap, prompting this ADR.
- raylib `raylib.h` — `CheckCollisionBoxes`/`Spheres`/`BoxSphere`, `GetRayCollisionSphere`/`Box`/
  `Mesh`/`Triangle`/`Quad` (raylib 6.0).
- [Jolt Physics](https://github.com/jrouwe/JoltPhysics), [Bullet](https://github.com/bulletphysics/bullet3) —
  the two real rigid-body engines compared above.
