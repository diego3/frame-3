# 2. Scene graph / transform hierarchy — build our own vs. adopt existing

- Status: Proposed
- Date: 2026-08-03

## Context

Following the *Game Coding Complete* (McShaffry & Graham) Ch. 9-10 discussion of scene graphs —
and the Actor-vs-ECS decision already made and recorded in
[ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) (ECS via
[EnTT](https://github.com/skypjack/entt), implemented directly on `main` in `603dcd2`) — this
project needs a way to attach entities to each other with composed transforms: a weapon in a
hand, a camera rig on a character, any case where one entity's world position/rotation should be
derived from a parent's, not stored independently.

This ADR surveys whether something ready-made should be adopted for this, or whether it should be
built from scratch on top of the already-chosen EnTT/raylib stack. ADR-0001 explicitly scoped
itself to the Actor-vs-ECS question and the `Engine` class, deferring this exact hierarchy
question rather than presupposing an answer — this ADR is that follow-up.

### Constraints that shape this decision

- **EnTT is already the chosen ECS, and non-negotiable for this decision specifically.**
  `.claude/skills/engine-architecture/SKILL.md`'s "Decisions Made" section is explicit that EnTT
  was picked over hand-rolling *and* deliberately not wrapped behind a swappable abstraction,
  because an ECS is the data layout and iteration paradigm gameplay code gets written directly
  against, not a swappable backend. Any option here that requires replacing EnTT re-opens a
  decision this project already closed for a stated reason, not a new one specific to hierarchy.
- **raylib is the renderer**, via `Vector3`/`Matrix`/`Quaternion` (`raymath.h`). Any option that
  brings its own renderer competes with raylib rather than sitting on top of it.
- **The project is a from-scratch, C++20, small-scale learning project** (a handful of entities
  per scene, not thousands) — explicitly not optimizing for a problem this project doesn't have.
- **`raylib_game_template`'s current single build system is a plain Makefile** (CMake was
  removed; see "Consolidate on the Makefile as the single build system"). A dependency that needs
  its own complex build (CMake subproject, custom toolchain) is a heavier lift here than it would
  be in a CMake-based project.

## Option 1: Build our own, fused into EnTT (recommended)

`Parent`/`LocalTransform`/`WorldTransform` components (or an intrusive relationship component —
see below) plus a topological transform-propagation system, using raylib's own
`Vector3`/`Quaternion`/`Matrix` math. This is the option already sketched (informally) in
`engine-architecture`'s §3 and in prior discussion of GCC's `SceneNode`, reimplemented as ECS
components + a system instead of an object tree.

**This isn't a shot in the dark — EnTT's own documentation directly addresses this.** From
EnTT's `docs/md/entity.md` ("Hierarchies and the like"):

> `EnTT` does not attempt in any way to offer built-in methods with hidden or unclear costs to
> facilitate the creation of hierarchies.

and it documents two concrete patterns to build on top, straight from the library's own docs:

```cpp
// Pattern A: intrusive doubly-linked list of children (EnTT's own suggested shape)
struct relationship {
    std::size_t children{};
    entt::entity first{entt::null};
    entt::entity prev{entt::null};
    entt::entity next{entt::null};
    entt::entity parent{entt::null};
    // ... other data members ...
};
```

```cpp
// Pattern B: stable raw pointers, enabled by in_place_delete (pointer-stable storage)
struct transform {
    static constexpr auto in_place_delete = true;
    transform *parent;
    // ... other data members ...
};
```

EnTT's docs recommend Pattern A's shape as the general answer, with Pattern B as an option when a
component type is visited mainly via hierarchy traversal or random access (its stability enables
holding a raw pointer safely across storage operations, which a resizable `std::vector<entity>` of
children could not). Either pattern is then driven by a small propagation system, run once per
frame before rendering, walking parents before children:

```cpp
// Sketch: propagate WorldTransform from LocalTransform + Parent's already-computed WorldTransform.
// Using registry.sort<Parent>(...) beforehand keeps this a single linear pass in topological
// order (parents before children) instead of needing per-entity recursive lookups.
struct LocalTransform { Vector3 position; Quaternion rotation; Vector3 scale; };
struct WorldTransform  { Matrix matrix; };

void PropagateTransforms(entt::registry& registry) {
    auto view = registry.view<LocalTransform, WorldTransform>();
    for (auto entity : view) {
        Matrix local = MatrixMultiply(
            MatrixMultiply(
                MatrixScale(view.get<LocalTransform>(entity).scale.x, /* ... */),
                QuaternionToMatrix(view.get<LocalTransform>(entity).rotation)),
            MatrixTranslate(/* ... */));
        if (auto* rel = registry.try_get<relationship>(entity); rel && rel->parent != entt::null) {
            local = MatrixMultiply(local, registry.get<WorldTransform>(rel->parent).matrix);
        }
        view.get<WorldTransform>(entity).matrix = local;
    }
}
```

| Pros | Cons |
|------|------|
| No new dependency, no build-system integration work — pure C++ on top of what's already wired in. | We own all of it — no community maintenance, no existing bug reports/fixes to inherit. |
| Grounded in EnTT's own documented guidance, not a novel invention — same shape EnTT's maintainers recommend for exactly this problem. | More upfront design work than "add a library" — need to actually implement and test the propagation system. |
| Composes naturally with raylib's math types (`Vector3`, `Quaternion`, `Matrix`) — no translation layer to a foreign math library. | |
| Scales down cleanly to this project's actual size (a handful of entities) — no unused generality. | |

## Option 2: Switch the ECS to flecs, for its built-in `ChildOf` relationship

[flecs](https://github.com/SanderMertens/flecs) has hierarchy as a first-class, built-in feature —
`ChildOf` is one of its two most common relationship uses, explicitly described in its own docs
as being used "for hierarchies like a scene graph." Queries can traverse `ChildOf` natively
(depth-first, continuing until a component is found or a root is reached), and flecs implements a
"reachable cache" specifically to make hierarchy-relative lookups fast without per-query manual
traversal.

| Pros | Cons |
|------|------|
| Hierarchy traversal and query-by-relationship come for free, well-optimized, from the library itself — not something we build or maintain. | **Requires abandoning EnTT**, a dependency already wired in (`603dcd2` on `main`, per ADR-0001) and already decided against being treated as swappable — this isn't a hierarchy decision, it's re-opening the ECS decision for a single feature. |
| Real, actively maintained project with first-party documentation of exactly this use case. | Different API/idioms from EnTT (flecs is more "framework," EnTT more "library primitives") — a bigger surface to learn than any hierarchy-specific decision should require. |

## Option 3: OpenSceneGraph (OSG)

A long-established, mature C++ scene graph toolkit (view-frustum/occlusion culling, LOD nodes,
OpenGL state sorting — real, deep scene-graph functionality). Its own project status: OSG is
described as "legacy... maintained in a backwards compatible fashion for existing applications,"
now in a maintenance phase, with [VulkanSceneGraph](https://vsg-dev.github.io/) positioned as its
modern successor (C++17, Vulkan, addresses OSG's CPU-bound performance issues).

| Pros | Cons |
|------|------|
| Genuinely full-featured — real culling, LOD, state sorting, decades of production use in simulation/vis-sim fields. | **Brings its own renderer.** Adopting OSG means either replacing raylib's rendering entirely or running two renderers side by side — directly against this project's raylib-first constraint. |
| | Legacy/maintenance-mode project by its own description — not where new work in the OSG family happens. |
| | No ECS integration story at all — would need its own from-scratch bridge to EnTT entities, on top of the renderer-replacement problem. |

## Option 4: VulkanSceneGraph (VSG) — OSG's modern successor

| Pros | Cons |
|------|------|
| Modern C++17, actively developed, addresses OSG's known CPU bottlenecks. | Vulkan, not OpenGL — raylib is an OpenGL-based renderer; VSG doesn't solve the "brings its own renderer" problem, it doubles it (now two different graphics APIs). |
| | Same "no ECS integration" gap as OSG. |

## Option 5: `rscenegraph.h` — a raylib-specific community scene graph

A real, targeted attempt at exactly this problem: a WIP scene graph with frustum culling,
built specifically for raylib
([SuperUserNameMan/rscenegraph.h](https://github.com/SuperUserNameMan/rscenegraph.h)).

| Pros | Cons |
|------|------|
| Only option researched that targets raylib specifically — no renderer mismatch. | **Archived and read-only** as of the repo's last update. The maintainer's own note: they were "currently evaluating migrating to Godot" due to concerns about raylib's maintenance burden. |
| | 1 star, 0 forks, 8 open issues — effectively unused outside its own author. |
| | README explicitly labels it WIP with no completion in sight ("I'll come back finishing this project ASAP" — never followed up before archival). |

**Not viable** — adopting an abandoned, single-author, explicitly-incomplete dependency (whose own
author left raylib) is a worse position than building the small amount of code this actually needs.

## A narrower, already-available primitive worth naming (not a substitute for the above)

raylib's own `Model` type already has a bone hierarchy for **skeletal animation** specifically:

```c
typedef struct BoneInfo {
    char name[32];
    int parent;             // Bone parent (index-based hierarchy)
} BoneInfo;

typedef struct ModelSkeleton {
    int boneCount;
    BoneInfo *bones;
    ModelAnimPose bindPose; // Transform[] — translation/rotation/scale per bone
} ModelSkeleton;
```

This is real, working parent-index hierarchy — but it's scoped strictly to the bones *within one
loaded model's skeleton*, for CPU/GPU skinning. It doesn't address attaching one entity to
another (a weapon `Model` to a hand bone, a camera to a character) — that's still Option 1's
problem to solve, and `Transform{translation, rotation, scale}` (raylib's own struct) is a
convenient, already-available local-transform shape to reuse in whatever component design Option
1 lands on.

## Comparison at a glance

| | 1: Build our own | 2: Switch to flecs | 3: OSG | 4: VSG | 5: rscenegraph.h |
|--|--|--|--|--|--|
| Works with EnTT (already decided) | Yes | No — replaces it | N/A (no ECS story) | N/A (no ECS story) | N/A (no ECS story) |
| Works with raylib's renderer | Yes | Yes (orthogonal to rendering) | No — own renderer | No — own renderer (Vulkan) | Yes (raylib-specific) |
| Actively maintained | N/A (ours) | Yes | Maintenance-mode | Yes | **No — archived** |
| New dependency / build integration | None | New ECS dependency | Heavy (own renderer) | Heavy (own renderer, Vulkan) | Would be, but not viable anyway |
| Fits project scale | Yes, by design | Overkill for hierarchy alone | Overkill | Overkill | N/A (abandoned) |

## Recommendation

**Option 1: build it ourselves**, directly on EnTT, grounded in EnTT's own documented
`relationship`/stable-pointer patterns rather than inventing a shape from nothing. None of the
"adopt something" options survive contact with this project's actual constraints — Options 3/4
fail the raylib-renderer constraint outright, Option 2 fails the already-decided
EnTT-is-not-swappable constraint, and Option 5 (the one option that would have fit both
constraints) is abandoned and not safe to depend on. Building this is a small, well-scoped task
given EnTT's documentation already describes the shape — not a large undertaking being avoided by
process of elimination.

## Open Questions

- **Pattern A (intrusive relationship component) vs. Pattern B (stable pointers via
  `in_place_delete`)** — EnTT's docs present both; Pattern A is likely the safer default (no
  storage-policy interaction to reason about), Pattern B may be worth it later if profiling shows
  hierarchy traversal is hot. Not decided; pick whichever is simpler to implement correctly first.
- **Should the transform-propagation system use `registry.sort<Parent>(...)` to maintain
  topological (parent-before-child) iteration order**, as noted in the engine-architecture skill?
  Worth confirming once real hierarchy depth exists to test against (a weapon-in-hand is depth 2;
  nothing deeper is planned yet).
- **How does this interact with `ModelSkeleton`'s bone hierarchy** for an entity whose `Model` is
  itself skinned/animated? Likely orthogonal (bone hierarchy stays internal to one `Model`'s
  skinning; entity-to-entity hierarchy is a separate concern) but not yet worked through.

## Consequences

### Positive

- No new dependency, no build-system integration, no license/maintenance risk from an external
  project (concretely demonstrated by Option 5's fate).
- Directly grounded in the ECS library already chosen, using patterns EnTT's own maintainers
  document for this exact problem — not a guess.

### Negative

- All maintenance burden is ours — no upstream bug fixes, no community-tested edge cases.
- Locks in "hierarchy lives in EnTT components + a system," which is the right call given the
  constraints above, but does mean any future ECS change (however unlikely, per the existing
  non-swappable-ECS decision) would need to rebuild this too.

## References

- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) — the Actor-vs-ECS decision (EnTT) and the
  `Engine` class, which this ADR follows up on directly.
- `.claude/skills/engine-architecture/SKILL.md` — the same decision as reflected in the
  engine-architecture skill's "Decisions Made"/"Open Questions" sections.
- `.claude/skills/engine-ai-behavior/SKILL.md` — related discussion of the GCC scene-graph
  concept adapted to ECS.
- EnTT `docs/md/entity.md`, "Hierarchies and the like" section — the `relationship`/stable-pointer
  patterns cited above, straight from the library's own documentation.
- [flecs Relationships docs](https://www.flecs.dev/flecs/md_docs_2Relationships.html) and
  [flecs Hierarchies docs](https://www.flecs.dev/flecs/md_docs_2HierarchiesManual.html) — `ChildOf`
  and the reachable-cache optimization.
- [OpenSceneGraph](https://github.com/openscenegraph/OpenSceneGraph) and
  [VulkanSceneGraph](https://vsg-dev.github.io/) — maintenance-mode status and the Vulkan-based
  successor project, respectively.
- [SuperUserNameMan/rscenegraph.h](https://github.com/SuperUserNameMan/rscenegraph.h) — archived,
  1 star, maintainer-stated move away from raylib.
- raylib `raylib.h` — `Model`, `BoneInfo`, `ModelSkeleton`, `Transform` struct definitions cited
  above (raylib 6.0).
- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 9-10 — the original `SceneNode`
  concept this ADR adapts to ECS.
