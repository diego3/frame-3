#include "doctest/doctest.h"

#include <entt/entt.hpp>
#include <raylib.h>
#include <raymath.h>

#include "app/scene/renderable.h"
#include "app/scene/transform.h"
#include "game/flare_reactor/beacon_pulse_process.h"
#include "game/flare_reactor/reactor.h"

TEST_CASE("BeaconPulseProcess animates scale/color mid-pulse and resets Reactor::active at the end") {
    entt::registry registry;
    entt::entity reactor = registry.create();
    registry.emplace<LocalTransform>(reactor);
    registry.emplace<WorldTransform>(reactor);
    registry.emplace<Renderable>(reactor, Renderable{Renderable::Shape::Box, Vector3{1, 1, 1}, GRAY, false});
    registry.emplace<Reactor>(reactor, Reactor{true});

    BeaconPulseProcess process(registry, reactor);

    process.Update(1.0f);   // halfway through the 2s pulse (flare_reactor_beacon_pulse::kDurationSeconds)
    CHECK_FALSE(process.IsDead());
    CHECK(registry.get<Reactor>(reactor).active == true);   // still mid-pulse -- not reset yet
    CHECK(registry.get<LocalTransform>(reactor).scale.x > 1.0f);   // grown past the base scale
    CHECK(registry.get<Renderable>(reactor).color.r != GRAY.r);    // shifted away from the base color

    process.Update(1.5f);   // pushes elapsed_ past kDurationSeconds
    CHECK(process.IsDead());
    CHECK(registry.get<Reactor>(reactor).active == false);   // BeaconPulseProcess resets it, not GameLogic
}

TEST_CASE("BeaconPulseProcess tolerates a reactor missing LocalTransform/Renderable") {
    entt::registry registry;
    entt::entity reactor = registry.create();
    registry.emplace<Reactor>(reactor, Reactor{true});

    BeaconPulseProcess process(registry, reactor);

    CHECK_NOTHROW(process.Update(flare_reactor_beacon_pulse::kDurationSeconds + 1.0f));
    CHECK(process.IsDead());
    CHECK(registry.get<Reactor>(reactor).active == false);
}
