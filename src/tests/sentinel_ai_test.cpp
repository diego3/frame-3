#include "doctest/doctest.h"

#include <entt/entt.hpp>
#include <raylib.h>
#include <raymath.h>

#include "app/scene/transform.h"
#include "game/flare_reactor/sentinel_ai.h"

TEST_CASE("UpdateSentinel Seeks the current waypoint and advances on arrival") {
    entt::registry registry;
    entt::entity sentinel = registry.create();
    registry.emplace<LocalTransform>(sentinel, LocalTransform{Vector3{0.0f, 0.0f, 0.0f}});
    registry.emplace<SentinelAI>(sentinel);
    registry.emplace<Patrol>(sentinel, Patrol{{Vector3{1.0f, 0.0f, 0.0f}, Vector3{-1.0f, 0.0f, 0.0f}}, 0});

    // Several ticks: enough to actually close the distance to waypoint 0, not just nudge toward it.
    for (int i = 0; i < 30; ++i) UpdateSentinel(registry, sentinel, 1.0f / 30.0f);

    CHECK(registry.get<SentinelAI>(sentinel).state == SentinelState::Patrol);
    CHECK(registry.get<Patrol>(sentinel).current == 1);   // arrived at waypoint 0, advanced to 1
}

TEST_CASE("UpdateSentinel Seeks investigateTarget while Investigating, without touching Patrol::current") {
    entt::registry registry;
    entt::entity sentinel = registry.create();
    registry.emplace<LocalTransform>(sentinel, LocalTransform{Vector3{0.0f, 0.0f, 0.0f}});
    registry.emplace<SentinelAI>(sentinel, SentinelAI{SentinelState::Investigate, Vector3{0, 0, 0}, Vector3{2.0f, 0.0f, 0.0f}});
    registry.emplace<Patrol>(sentinel, Patrol{{Vector3{-5.0f, 0.0f, 0.0f}}, 0});

    UpdateSentinel(registry, sentinel, 0.1f);

    CHECK(registry.get<LocalTransform>(sentinel).position.x > 0.0f);   // moved toward investigateTarget
    CHECK(registry.get<Patrol>(sentinel).current == 0);                // Patrol untouched while Investigating
}

TEST_CASE("UpdateSentinel does nothing for an entity missing SentinelAI or LocalTransform") {
    entt::registry registry;
    entt::entity bare = registry.create();

    CHECK_NOTHROW(UpdateSentinel(registry, bare, 0.1f));
}

TEST_CASE("ApplyBeaconPerception switches a nearby sentinel to Investigate and leaves a far one alone") {
    entt::registry registry;

    entt::entity near = registry.create();
    registry.emplace<LocalTransform>(near, LocalTransform{Vector3{1.0f, 0.0f, 0.0f}});
    registry.emplace<SentinelAI>(near);

    entt::entity far = registry.create();
    registry.emplace<LocalTransform>(far, LocalTransform{Vector3{100.0f, 0.0f, 0.0f}});
    registry.emplace<SentinelAI>(far);

    ApplyBeaconPerception(registry, Vector3{0.0f, 0.0f, 0.0f}, /*hearingRadius=*/10.0f);

    CHECK(registry.get<SentinelAI>(near).state == SentinelState::Investigate);
    CHECK(Vector3Equals(registry.get<SentinelAI>(near).investigateTarget, Vector3{0.0f, 0.0f, 0.0f}));
    CHECK(registry.get<SentinelAI>(far).state == SentinelState::Patrol);
}
