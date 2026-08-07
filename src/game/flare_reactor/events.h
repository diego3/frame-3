// Events for the flare_reactor experiment's interact flow (docs/rfc/0001-flare-reactor-pipeline-
// experiment.md, Phase 3). Two hops, matching the RFC's own §2/§4 design: PlayerInteractElement
// emits EvtData_ActivateBeacon unconditionally on ACTION_INTERACT (raw intent, no validation --
// same division TeapotController.cpp has in the book: input translates a key to an event, never
// decides if it's *allowed*); FlareReactorGameLogic::OnActivateBeacon validates it (proximity,
// reactor not already active) and, only if granted, emits EvtData_BeaconTriggered.
#ifndef FLARE_REACTOR_EVENTS_H
#define FLARE_REACTOR_EVENTS_H

#include <entt/entt.hpp>
#include <raylib.h>

struct EvtData_ActivateBeacon {
    entt::entity actorId;
};

// Carries reactorId's position at the moment of triggering -- a one-shot snapshot for whoever
// reacts (Phase 4's pulse VFX, Phase 6's SentinelAI perception), not a second live source of
// truth for it (that stays WorldTransform, ADR-0002); nothing re-reads this field after the event
// is handled, so it never has a chance to go stale.
struct EvtData_BeaconTriggered {
    entt::entity reactorId;
    Vector3 position;
};

#endif // FLARE_REACTOR_EVENTS_H
