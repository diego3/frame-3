# 12. Physics: thin raylib-collision layer behind an `IGamePhysics`-shaped seam, real dynamics deferred

- Status: Proposed
- Date: 2026-08-05

## Context

Nothing in this project addresses physics or collision in any form — no ADR, no skill, no library
choice, not even a mention. Every other Game Coding Complete-derived system in frame-3 has at least
been evaluated (event/process manager, resources, entity loading, level loading, and now the
Logic/View split itself — [ADR-0010](0010-base-game-logic-and-igameview.md)); physics hasn't, and
[`docs/roadmap.md`](../roadmap.md) still flags it as the highest-impact remaining gap. This ADR is
that evaluation. (An earlier draft of this ADR was written and numbered before ADR-0010 existed;
renumbered to 0012 and revised below to account for it, and for ADR-0008/ADR-0002 having since
shipped rather than merely being proposed.)

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
Makefile-only build" lens ADR-0004 (resources) and ADR-0008 (data-driven entity loading, now
shipped) already applied. A third question is new since the earlier draft of this ADR: how physics
fits into [ADR-0010](0010-base-game-logic-and-igameview.md)'s now-decided `BaseGameLogic`/
`IGameView` split, since that ADR didn't exist when physics was first evaluated.

## Decision — collision detection only, via raylib's own primitives, behind a kept `IGamePhysics` seam

### raylib already does real (if basic) 3D collision detection

Unlike a texture/model loader (ADR-0004) or an entity/level definition parser (ADR-0008), raylib
doesn't ship a rigid-body dynamics engine — but it does ship real collision-detection primitives,
as part of core `raylib.h`, not a separate module: `CheckCollisionBoxes`, `CheckCollisionSpheres`,
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
// Paired with the entity's WorldTransform (transform.h, ADR-0002 -- shipped), not LocalTransform:
// a collider needs world-space position after hierarchy propagation, not a component's position
// relative to its own parent. The center comes from WorldTransform.matrix's translation
// (Vector3Transform(Vector3Zero(), matrix)) -- a collider doesn't carry its own position. Note
// WorldTransform.matrix can encode rotation, but raylib's CheckCollisionBoxes takes an
// axis-aligned BoundingBox (min/max only, no orientation) -- see Tradeoffs for what that costs.
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

### Where this lives: a `BaseGameLogic` member, not an `Engine` member

Physics simulation is a Game Logic/simulation concern, not an Application-layer one — `Engine`
deliberately doesn't own Game Logic+View concerns (ADR-0001 Decision 2). The original draft of
this ADR (written before ADR-0010 existed) proposed `IGamePhysics`/`RaylibCollisionPhysics` as a
standalone object the `screen_gameplay.c` bridge constructs alongside `BaseGameLogic`. Now that
[ADR-0010](0010-base-game-logic-and-igameview.md) has decided `BaseGameLogic`'s actual shape —
holding `entt::registry&`/`EventManager&`/`ProcessManager&`/`LevelLoader&` by reference and ticking
attached views from its own `VOnUpdate(dt)` — physics fits that same shape better than sitting
beside it: `BaseGameLogic` owns a `std::unique_ptr<IGamePhysics>` and calls
`physics_->Update(registry_, events_, dt)` from inside its own `VOnUpdate`, the same place
`LevelLoader`-driven spawning and view updates already happen. This is a **change from the
original draft**, made possible only because ADR-0010 now exists to define what "the thing that
owns simulation-tick-order" actually is in this codebase.

## Options considered for adopting a real rigid-body engine now instead

Four real third-party rigid-body engines were surveyed, spanning "smallest real engine" to
"heaviest, most enterprise," plus the two non-engine options (today's recommendation, and building
dynamics ourselves):

### `ReactPhysics3D`

A deliberately lightweight, real-time 3D physics library — rigid bodies, joints (ball-and-socket,
hinge, slider, fixed), and a modest set of collision shapes (box, sphere, capsule, convex mesh,
height field). zlib license. Smaller community than the other three engines below, and not used in
any AAA title this ADR is aware of, but genuinely maintained and genuinely a real rigid-body
engine, not a toy. Its codebase is meaningfully smaller than Bullet/Jolt/PhysX's, which matters
specifically for this project's build constraint (see below).

### Jolt Physics

Modern C++17/20, MIT license, actively maintained by Jorrit Rouwe (a former Guerrilla Games
physics programmer) and used in shipped AAA titles (Horizon Forbidden West). Full rigid-body
dynamics: forces, constraints/joints, continuous collision detection, designed from the start for
high entity counts and multithreading. Widely regarded today as the strongest modern option on
technical merit among open-source C++ physics engines.

### Bullet (the book's own pick)

zlib license, in active (if slower-paced than Jolt's) community maintenance
(`bulletphysics/bullet3`). The most battle-tested and widely deployed of the four historically —
used across countless games and tools (Blender's physics, for one) since the mid-2000s. Broader
niche feature coverage than Jolt in some areas (e.g. soft-body simulation), at the cost of an older
(C++03-era) codebase and API style.

### PhysX (NVIDIA)

BSD-3-Clause (open-sourced since v4/v5). The default physics engine behind Unreal Engine and many
other AAA titles; NVIDIA-maintained, with a lineage that includes optional GPU-accelerated rigid
body simulation. The heaviest option surveyed both in capability and in integration cost: its build
isn't a plain CMake invocation the way Jolt's/Bullet's/ReactPhysics3D's are — it's generated via
NVIDIA's own Python-driven preset scripts that call CMake underneath, one more moving part than the
other three.

### Comparison

| | **raylib collision only** (recommended) | **ReactPhysics3D** | **Jolt Physics** | **Bullet** (book's pick) | **PhysX** | **Hand-rolled dynamics** |
|---|---|---|---|---|---|---|
| License | N/A (already raylib) | zlib | MIT | zlib | BSD-3-Clause | N/A |
| Codebase era / style | N/A | Modern C++ | Modern C++17/20 | Older, C++03-era roots | Modern C++, NVIDIA house style | Whatever we write |
| Build system | None | CMake | CMake | CMake (legacy Makefiles exist, unmaintained) | CMake, via NVIDIA's own Python preset-generation scripts — the heaviest build of the four | None |
| Fits Makefile-only build (ADR-0001 Decision 3, ADR-0006) | Yes, trivially | **No** as-is, but its smaller codebase makes a hand-written "compile every `.cpp`" Makefile rule the most *plausible* of the four to pull off without CMake at all | **No** as-is | **No** as-is | **No** as-is, and the extra preset-generation layer makes even a CMake-invoked-from-`build.sh` step harder than for the other three | Yes |
| Maintenance / community | N/A | Real, but smaller than the other three; no major shipped-title track record found | Very active; strong recent track record (Horizon Forbidden West) | Active but slower-paced than Jolt; the longest track record of the four | Very active; NVIDIA-backed | None — we'd own every bug |
| Capability | Overlap checks + raycasts only — no forces, mass, momentum, joints, continuous collision detection | Real rigid-body dynamics, joints, a modest shape set — the smallest *real* engine of the four | Full rigid-body dynamics, CCD, built for high entity counts/multithreading | Full rigid-body dynamics, broadest niche coverage (e.g. soft-body) of the four | Full rigid-body dynamics, the most capable/heaviest, optional GPU acceleration lineage | Whatever we implement — a real dynamics/constraint solver is a substantial, error-prone undertaking serious engines spend years on |
| Matches this project's current need | Yes — no gameplay exists yet; likely first real needs are "did the capsule hit a wall," not realistic momentum transfer | Closest-fitting *real engine* if/when dynamics is needed but full AAA feature breadth isn't | Best technical pick if capability/performance matters more than build-footprint | The book's own choice; still viable, just no longer the clear technical leader | Overkill for this project at any foreseeable scale | Rejected — unlike the scene graph (ADR-0002, genuinely small/well-scoped), rigid-body dynamics is not a small problem |

**Note on the Makefile-only constraint and physics specifically**: unlike the YAML case
(ADR-0008), where a genuinely simpler, equally-fit-for-purpose header-only alternative
(mini-yaml) existed, there is no realistic "mini-yaml of rigid-body physics" among the four real
engines surveyed — all of them are CMake-based, because the problem itself (broad-phase,
narrow-phase, SIMD, constraint solving) is bigger than what a header-only library reasonably
covers. When real dynamics is actually needed, this project will have to pick one of:

1. **Invoke the chosen engine's own CMake as a one-time `build.sh` preprocessing step**, producing
   a `.a` to link normally — the same *shape* raylib's own Makefile-invoked-from-`build.sh` already
   uses, but a CMake invocation specifically, which ADR-0006 previously avoided even as a one-time
   step by picking doctest over GoogleTest instead.
2. **Hand-write a Makefile rule compiling the engine's sources directly**, skipping its build
   system entirely — possible in principle (compiling every `.cpp` with consistent flags doesn't
   strictly require the upstream build system), but riskier the larger and more
   platform/SIMD/precision-feature-detecting the target's CMake logic is. This is meaningfully more
   *plausible* for **ReactPhysics3D** specifically (smallest of the four codebases, least exotic
   build-time feature detection) than for Jolt, Bullet, or especially PhysX (whose build is the
   heaviest of the four even by CMake's own standards).

Recorded now so this cost is visible before it's paid, not discovered by surprise in a future ADR.

## Tradeoffs accepted

- No dynamics: no gravity-as-a-force, no momentum/velocity integration, no realistic collision
  response, no continuous collision detection (a fast-moving object can tunnel through a thin
  collider within one frame) — accepted because no gameplay exists yet to need any of this;
  revisit once a concrete case (a projectile that needs to reliably hit a thin wall, physically
  plausible knockback, ragdolls, vehicle physics) actually shows up.
- Only box/sphere collider shapes — raylib's own collision functions don't cover arbitrary convex
  hulls or capsules; accepted since most gameplay starts with primitive collider shapes anyway.
- `BoxCollider` is treated as **axis-aligned** in world space — raylib's `CheckCollisionBoxes`
  takes a `BoundingBox` (min/max corners only), so any rotation baked into an entity's
  `WorldTransform.matrix` is ignored for box-shaped colliders (spheres are unaffected — a sphere
  looks the same rotated or not). Accepted since most gameplay starts with unrotated or
  rotation-insensitive colliders (a rotating platform's *box* wouldn't be handled correctly, but a
  static wall or an axis-aligned pickup volume would); revisit with a real oriented-bounding-box
  check (not something raylib provides out of the box) if a concrete case needs it.
- Pairwise (`O(n²)`) overlap checking, no broad-phase spatial partitioning (grid/BVH) — accepted at
  today's near-zero entity count; the frame-budget SLO (`.claude/skills/engine-sre`) is the
  existing, already-tracked signal for when this stops being fine.
- Movement/position integration is explicitly **not** `IGamePhysics`'s job here (unlike the book's
  real-Bullet-backed implementation, which does own velocity/force integration) — this ADR is
  collision detection only; wherever gameplay code currently moves entities keeps doing so.
- The Makefile-only constraint likely doesn't survive contact with a real physics engine adoption
  later — flagged above rather than assumed away.
- `IGamePhysics` is owned by `BaseGameLogic` (ADR-0010), not constructed independently by the
  `screen_gameplay.c` bridge — a change from this ADR's original draft, made because ADR-0010 now
  defines what owns simulation tick order; `BaseGameLogic`'s own shape isn't built yet either, so
  this is still a design choice on paper, not verified against real code.

## Consequences / follow-ups

- `.claude/skills/engine-architecture` should gain a physics section once
  `IGamePhysics`/`RaylibCollisionPhysics`/`Collider` components land in code.
- `docs/roadmap.md`: move "Physics / collision" from "Not started" to "Proposed" pointing at this
  ADR once merged; move it to "Shipped" once the code lands, per that file's own stated convention.
- **Sequencing, same shape as ADR-0010's own note**: this ADR can be reviewed/accepted now, but
  implementation should wait for `BaseGameLogic` (ADR-0010, itself waiting on ADR-0009/`LevelLoader`)
  to exist first — `BaseGameLogic::VOnUpdate` has nothing to call `physics_->Update(...)` from
  otherwise. Collider components and `RaylibCollisionPhysics`'s pure collision-check logic could
  still be written and unit-tested independently of that sequencing (matching how
  `resource_cache_test.cpp`/`entity_def_test.cpp` test their systems without raylib/window
  dependencies) — only the "where it's ticked from" piece is blocked.
- The first real gameplay entity with a collider is also the first test of whether
  `EvtData_CollisionBegin`/`End`'s pairwise-entity shape is the right granularity, or whether a
  higher-level "hit" concept (with damage/impulse data attached) belongs on top of it.
- When real dynamics is actually needed, this ADR leans toward two candidates depending on what
  actually drives the decision: **Jolt** if capability/performance/modern-C++ fit matters most,
  **ReactPhysics3D** if minimizing vendoring/build footprint matters most (its smaller codebase is
  the one most plausibly hand-compiled without CMake at all, per the note above). Bullet remains
  viable (the book's own pick, longest track record) but is no longer the clear technical leader
  among the four; PhysX is judged overkill for this project at any foreseeable scale. Whichever
  gets picked, the CMake-vendoring problem needs its own resolution first, as its own ADR, not
  assumed away by this one.

## Open Questions

- **Should collider shapes be extended (capsule, convex hull) before real dynamics arrives**, using
  more of raylib's own primitives (e.g. `GetRayCollisionMesh` for arbitrary meshes), or should that
  wait for a real engine? Not decided; likely depends on what the first few real entities actually
  need.
- ~~How does this interact with the still-undecided scene graph (ADR-0002)?~~ **Resolved** while
  updating this ADR: ADR-0002 shipped (`Relationship`/`LocalTransform`/`WorldTransform`,
  `PropagateTransforms`) since the earlier draft — colliders pair with `WorldTransform` directly
  (see the ECS mapping above), and `Engine::Run`'s `TickAndUpdateDraw` already recomputes every
  entity's `WorldTransform` before `updateAndDraw()` runs each frame, so by the time
  `BaseGameLogic::VOnUpdate` (and the physics tick inside it) runs, `WorldTransform` is already
  current for that frame — no additional ordering concern to design here.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham) — `IGamePhysics`, `NullPhysics`,
  `PhysicsComponent`, `EvtData_PhysCollision`/`EvtData_PhysSeparation`, the Bullet-backed
  implementation.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) — ECS via EnTT (Decision 1, the
  Actor→component re-mapping this ADR follows) and `Engine`'s Ch. 5-only scope (Decision 2, why
  physics isn't an `Engine` member).
- [ADR-0002](0002-scene-graph-hierarchy-options.md) — `LocalTransform`/`WorldTransform`/
  `Relationship`/`PropagateTransforms`, shipped; `WorldTransform` is what a `Collider` pairs with.
- [ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) — "raylib already does the hard
  part" reasoning, reapplied here to collision detection; the kept-seam-even-when-thin precedent.
- [ADR-0006](0006-doctest-for-unit-tests.md) — the Makefile-only vendoring constraint (no CMake)
  this ADR's engine comparison reapplies, and the precedent for rejecting a CMake-only dependency
  even as a one-time `build.sh` step.
- [ADR-0008](0008-data-driven-entity-loading-yaml.md) — the format-swappable-seam reasoning reused
  here for `IGamePhysics`; `EntityFactory`/`EntityDefNode`/`IEntityFileParser` shipped since this
  ADR's earlier draft.
- [ADR-0009](0009-level-loading-actor-placement.md) — the "fire lifecycle events unconditionally,
  even with no subscriber yet" reasoning reused here for `EvtData_CollisionBegin`/`End`.
- [ADR-0010](0010-base-game-logic-and-igameview.md) — `BaseGameLogic`, decided after this ADR's
  original draft; now where `IGamePhysics` is owned and ticked from, per the updated "Where this
  lives" section above.
- [`docs/roadmap.md`](../roadmap.md) — flagged this as the top unaddressed gap, prompting this ADR.
- raylib `raylib.h` — `CheckCollisionBoxes`/`Spheres`/`BoxSphere`, `GetRayCollisionSphere`/`Box`/
  `Mesh`/`Triangle`/`Quad` (raylib 6.0).
- [Jolt Physics](https://github.com/jrouwe/JoltPhysics),
  [Bullet](https://github.com/bulletphysics/bullet3),
  [PhysX](https://github.com/NVIDIA-Omniverse/PhysX),
  [ReactPhysics3D](https://github.com/DanielChappuis/reactphysics3d) — the four real rigid-body
  engines compared above.
