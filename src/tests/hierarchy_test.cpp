#include "doctest/doctest.h"

#include <entt/entt.hpp>

#include "app/hierarchy.h"
#include "app/transform.h"

namespace {
    // Reads the world-space position PropagateTransforms computed for `entity` by transforming
    // the origin through its WorldTransform -- avoids reaching into Matrix fields directly, and
    // reads exactly what a renderer would (where does this entity actually end up).
    Vector3 WorldPosition(entt::registry &registry, entt::entity entity) {
        return Vector3Transform(Vector3{0.0f, 0.0f, 0.0f}, registry.get<WorldTransform>(entity).matrix);
    }

    bool NearlyEqual(Vector3 a, Vector3 b) {
        return Vector3Distance(a, b) < 0.0001f;
    }
}

TEST_CASE("SetParent links child under parent") {
    entt::registry registry;
    entt::entity parent = registry.create();
    entt::entity child = registry.create();

    SetParent(registry, child, parent);

    CHECK(registry.get<Relationship>(child).parent == parent);
    CHECK(registry.get<Relationship>(parent).first == child);
    CHECK(registry.get<Relationship>(parent).children == 1);
}

TEST_CASE("A second child becomes the new first, preserving the sibling chain") {
    entt::registry registry;
    entt::entity parent = registry.create();
    entt::entity childA = registry.create();
    entt::entity childB = registry.create();

    SetParent(registry, childA, parent);
    SetParent(registry, childB, parent);

    CHECK(registry.get<Relationship>(parent).children == 2);
    CHECK(registry.get<Relationship>(parent).first == childB);
    CHECK(registry.get<Relationship>(childB).next == childA);
    CHECK(registry.get<Relationship>(childA).prev == childB);
}

TEST_CASE("RemoveParent detaches a child and decrements the parent's count") {
    entt::registry registry;
    entt::entity parent = registry.create();
    entt::entity child = registry.create();
    SetParent(registry, child, parent);

    RemoveParent(registry, child);

    CHECK(registry.get<Relationship>(child).parent == entt::null);
    CHECK(registry.get<Relationship>(parent).children == 0);
    CHECK(registry.get<Relationship>(parent).first == entt::null);
}

TEST_CASE("Removing a middle child preserves its siblings' links") {
    entt::registry registry;
    entt::entity parent = registry.create();
    entt::entity childA = registry.create();
    entt::entity childB = registry.create();
    entt::entity childC = registry.create();

    // Chain ends up parent -> C -> B -> A (each SetParent prepends).
    SetParent(registry, childA, parent);
    SetParent(registry, childB, parent);
    SetParent(registry, childC, parent);

    RemoveParent(registry, childB);

    CHECK(registry.get<Relationship>(parent).children == 2);
    CHECK(registry.get<Relationship>(childC).next == childA);
    CHECK(registry.get<Relationship>(childA).prev == childC);
}

TEST_CASE("Re-parenting detaches from the old parent automatically") {
    entt::registry registry;
    entt::entity parentA = registry.create();
    entt::entity parentB = registry.create();
    entt::entity child = registry.create();

    SetParent(registry, child, parentA);
    SetParent(registry, child, parentB);

    CHECK(registry.get<Relationship>(parentA).children == 0);
    CHECK(registry.get<Relationship>(parentB).children == 1);
    CHECK(registry.get<Relationship>(child).parent == parentB);
}

TEST_CASE("PropagateTransforms computes a standalone root's WorldTransform from its LocalTransform") {
    entt::registry registry;
    entt::entity entity = registry.create();
    registry.emplace<LocalTransform>(entity).position = Vector3{1.0f, 2.0f, 3.0f};
    registry.emplace<WorldTransform>(entity);

    PropagateTransforms(registry);

    CHECK(NearlyEqual(WorldPosition(registry, entity), Vector3{1.0f, 2.0f, 3.0f}));
}

TEST_CASE("PropagateTransforms composes a child's world position with its parent's") {
    entt::registry registry;

    entt::entity parent = registry.create();
    registry.emplace<LocalTransform>(parent).position = Vector3{10.0f, 0.0f, 0.0f};
    registry.emplace<WorldTransform>(parent);

    entt::entity child = registry.create();
    registry.emplace<LocalTransform>(child).position = Vector3{1.0f, 0.0f, 0.0f};
    registry.emplace<WorldTransform>(child);

    SetParent(registry, child, parent);

    PropagateTransforms(registry);

    CHECK(NearlyEqual(WorldPosition(registry, parent), Vector3{10.0f, 0.0f, 0.0f}));
    CHECK(NearlyEqual(WorldPosition(registry, child), Vector3{11.0f, 0.0f, 0.0f}));
}

TEST_CASE("PropagateTransforms is correct at three levels deep regardless of creation/attach order") {
    entt::registry registry;

    // Deliberately created and attached out of hierarchy order (child before parent before
    // grandparent) -- PropagateTransforms must not depend on entt's internal storage order
    // happening to match parent-before-child, only on walking down from real roots.
    entt::entity child = registry.create();
    registry.emplace<LocalTransform>(child).position = Vector3{1.0f, 0.0f, 0.0f};
    registry.emplace<WorldTransform>(child);

    entt::entity parent = registry.create();
    registry.emplace<LocalTransform>(parent).position = Vector3{0.0f, 10.0f, 0.0f};
    registry.emplace<WorldTransform>(parent);

    entt::entity grandparent = registry.create();
    registry.emplace<LocalTransform>(grandparent).position = Vector3{0.0f, 0.0f, 100.0f};
    registry.emplace<WorldTransform>(grandparent);

    SetParent(registry, child, parent);
    SetParent(registry, parent, grandparent);

    PropagateTransforms(registry);

    CHECK(NearlyEqual(WorldPosition(registry, grandparent), Vector3{0.0f, 0.0f, 100.0f}));
    CHECK(NearlyEqual(WorldPosition(registry, parent), Vector3{0.0f, 10.0f, 100.0f}));
    CHECK(NearlyEqual(WorldPosition(registry, child), Vector3{1.0f, 10.0f, 100.0f}));
}

TEST_CASE("A Relationship-only root with no Transform still propagates its children") {
    entt::registry registry;

    // A purely organizational "rig" entity: has children, but no Transform of its own.
    entt::entity rig = registry.create();

    entt::entity child = registry.create();
    registry.emplace<LocalTransform>(child).position = Vector3{5.0f, 5.0f, 5.0f};
    registry.emplace<WorldTransform>(child);

    SetParent(registry, child, rig);

    PropagateTransforms(registry);

    CHECK(NearlyEqual(WorldPosition(registry, child), Vector3{5.0f, 5.0f, 5.0f}));
}
