// NOTE: calls TraceLog (real raylib runtime symbol), so -- same reasoning already documented in
// src/Makefile for game/sandbox/human_view.cpp/screen_gameplay.cpp -- this deliberately doesn't
// get a tests/-local object; the test build never links -lraylib. The validation math itself
// (Vector3Distance, the active-flag check) is pure and would be easy to unit test in isolation,
// but splitting it into a separate raylib-free header just to make that possible is disproportionate
// for one TraceLog call proving Phase 3's flow, per the RFC ("um TraceLog prova o fluxo"). Revisit
// if this grows real branching logic worth testing on its own.
#include "game_logic.h"

#include <memory>

#include <raymath.h>

#include "app/scene/transform.h"
#include "beacon_pulse_process.h"
#include "reactor.h"

namespace {
    // No physics/collision yet (RFC-0001's "Colisão/proximidade real" gap) -- a flat WorldTransform
    // distance check is this experiment's stand-in, same flat-plane assumption FlareReactorView's
    // own movement already makes.
    constexpr float kInteractRadius = 3.0f;

    Vector3 WorldPosition(entt::registry &registry, entt::entity entity) {
        const WorldTransform &world = registry.get<WorldTransform>(entity);
        return Vector3Transform(Vector3Zero(), world.matrix);
    }
}

FlareReactorGameLogic::FlareReactorGameLogic(entt::registry &registry, EventManager &events,
                                              ProcessManager &processes, LevelLoader &levelLoader)
    : BaseGameLogic(registry, events, processes, levelLoader) {
    events_.Subscribe<EvtData_ActivateBeacon>(
        [this](const EvtData_ActivateBeacon &event) { OnActivateBeacon(event); });
}

void FlareReactorGameLogic::OnActivateBeacon(const EvtData_ActivateBeacon &event) {
    if (!registry_.valid(event.actorId)) return;

    Vector3 actorPos = WorldPosition(registry_, event.actorId);

    auto reactors = registry_.view<Reactor, WorldTransform>();
    for (auto entity : reactors) {
        auto &reactor = reactors.get<Reactor>(entity);
        if (reactor.active) continue;   // already triggered -- Phase 4's BeaconPulseProcess resets it

        Vector3 reactorPos = WorldPosition(registry_, entity);
        if (Vector3Distance(actorPos, reactorPos) > kInteractRadius) continue;

        reactor.active = true;
        processes_.Attach(std::make_unique<BeaconPulseProcess>(registry_, entity));
        // Queue, not Emit -- ADR-0005's reentrancy protection for "one event, several decoupled
        // subscribers" (RFC-0001 §3), even though no planned subscriber re-emits this same type
        // today.
        events_.Queue(EvtData_BeaconTriggered{entity, reactorPos});

        TraceLog(LOG_INFO, "FlareReactorGameLogic: reactor activated (BeaconPulseProcess attached, "
                            "EvtData_BeaconTriggered queued)");
        return;   // only one reactor in this scenario -- stop once it's handled
    }
}
