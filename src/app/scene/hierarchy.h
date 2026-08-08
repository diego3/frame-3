// Entity-to-entity transform hierarchy (docs/adr/0002, Option 1: build our own, fused into EnTT --
// not a scene-graph library, not a switch to a different ECS). Relationship is EnTT's own
// documented "Pattern A" shape for this (entt docs/md/entity.md, "Hierarchies and the like"): an
// intrusive doubly-linked list of children. Chosen over the stable-pointer "Pattern B" per
// ADR-0002's Open Questions -- no storage-policy interaction (in_place_delete) to reason about,
// simplest to implement correctly first; revisit only if profiling ever shows hierarchy traversal
// is hot at this project's scale (a handful of entities per scene).
#ifndef HIERARCHY_H
#define HIERARCHY_H

#include <entt/entt.hpp>

#include "app/scene/transform.h"

struct Relationship {
    std::size_t children = 0;
    entt::entity first = entt::null;
    entt::entity prev = entt::null;
    entt::entity next = entt::null;
    entt::entity parent = entt::null;
};

// Detaches child from its current parent, if any, patching up the old parent's child list. A
// no-op if child has no Relationship, or is already a root. Leaves child's own Relationship in
// place (its own children/first are unaffected) -- only its link to its former parent is removed.
inline void RemoveParent(entt::registry &registry, entt::entity child) {
    Relationship *rel = registry.try_get<Relationship>(child);
    if (rel == nullptr || rel->parent == entt::null) return;

    Relationship &parentRel = registry.get<Relationship>(rel->parent);

    if (rel->prev != entt::null) registry.get<Relationship>(rel->prev).next = rel->next;
    else parentRel.first = rel->next;

    if (rel->next != entt::null) registry.get<Relationship>(rel->next).prev = rel->prev;

    parentRel.children--;

    rel->parent = entt::null;
    rel->prev = entt::null;
    rel->next = entt::null;
}

// Attaches child to parent as its new first child, detaching child from any previous parent first
// (re-parenting is just RemoveParent + re-link, not a separate operation to reason about).
// Emplaces a default Relationship on either entity that doesn't already have one -- a plain
// entt::entity with no Relationship at all is implicitly a root with no children, exactly like one
// with an emplaced-but-empty Relationship, so callers never need to check which case applies.
inline void SetParent(entt::registry &registry, entt::entity child, entt::entity parent) {
    RemoveParent(registry, child);

    // Both get_or_emplace calls happen before either reference below is taken: emplacing parent's
    // Relationship after already holding a reference to child's could reallocate the shared
    // storage and dangle it, since Relationship isn't pointer-stable (that's Pattern B, not this).
    registry.get_or_emplace<Relationship>(child);
    registry.get_or_emplace<Relationship>(parent);

    Relationship &childRel = registry.get<Relationship>(child);
    Relationship &parentRel = registry.get<Relationship>(parent);

    childRel.parent = parent;
    childRel.prev = entt::null;
    childRel.next = parentRel.first;

    if (parentRel.first != entt::null) registry.get<Relationship>(parentRel.first).prev = child;

    parentRel.first = child;
    parentRel.children++;
}

namespace detail {
    inline Matrix ComputeLocalMatrix(const LocalTransform &t) {
        Matrix scale = MatrixScale(t.scale.x, t.scale.y, t.scale.z);
        Matrix rotation = QuaternionToMatrix(t.rotation);
        Matrix translation = MatrixTranslate(t.position.x, t.position.y, t.position.z);
        return MatrixMultiply(MatrixMultiply(scale, rotation), translation);
    }

    // Recurses down from `entity`, computing WorldTransform = LocalTransform * parentWorld for it
    // (if it has both components -- an entity with only a Relationship, no Transform, is a valid
    // organizational node that just passes parentWorld through unchanged to its own children) and
    // every descendant reachable through Relationship. A child's WorldTransform is only ever
    // written after its parent's own has already been computed by this same call, regardless of
    // what order EnTT happens to store either entity in internally -- the property
    // registry.sort<Relationship>(...) would otherwise be needed for (ADR-0002's Open Questions).
    inline void PropagateFrom(entt::registry &registry, entt::entity entity, const Matrix &parentWorld) {
        Matrix world = parentWorld;

        if (auto *local = registry.try_get<LocalTransform>(entity)) {
            if (auto *worldTransform = registry.try_get<WorldTransform>(entity)) {
                worldTransform->matrix = MatrixMultiply(ComputeLocalMatrix(*local), parentWorld);
                world = worldTransform->matrix;
            }
        }

        if (const Relationship *rel = registry.try_get<Relationship>(entity)) {
            for (entt::entity child = rel->first; child != entt::null;) {
                entt::entity next = registry.get<Relationship>(child).next;
                PropagateFrom(registry, child, world);
                child = next;
            }
        }
    }
}

// Computes WorldTransform for every entity reachable from a hierarchy root, by walking down from
// each one (see PropagateFrom above for why this doesn't need registry.sort<Relationship>(...)).
// Intended to be ticked once per frame, before drawing (Engine::Run's TickAndUpdateDraw,
// src/app/engine.cpp).
//
// Two disjoint groups of roots, since neither view alone covers both:
//   1. Entities with a Relationship but no parent -- the top of some hierarchy, whether or not the
//      root itself has a Transform (e.g. a purely organizational "rig" entity).
//   2. Entities with a Transform but no Relationship at all -- never reached by (1), since they
//      have no Relationship component for that view to find them through.
inline void PropagateTransforms(entt::registry &registry) {
    auto hierarchyRoots = registry.view<Relationship>();
    for (auto entity : hierarchyRoots) {
        if (hierarchyRoots.get<Relationship>(entity).parent == entt::null) {
            detail::PropagateFrom(registry, entity, MatrixIdentity());
        }
    }

    auto standaloneRoots = registry.view<LocalTransform, WorldTransform>(entt::exclude<Relationship>);
    for (auto entity : standaloneRoots) {
        detail::PropagateFrom(registry, entity, MatrixIdentity());
    }
}

#endif // HIERARCHY_H
