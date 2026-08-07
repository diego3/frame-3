// FlareReactorGameLogic: this experiment's first game-specific BaseGameLogic subclass
// (docs/rfc/0001-flare-reactor-pipeline-experiment.md, Phase 3 -- resolves the "onde mora o
// Subscribe de GameLogic" question the RFC's own "Questões em aberto" left open). Constructor
// subscribes two rules:
//   - EvtData_ActivateBeacon -> OnActivateBeacon: validates proximity + reactor not already
//     active before granting it as EvtData_BeaconTriggered.
//   - EvtData_BeaconTriggered -> OnBeaconTriggered (Phase 6): broadcast perception for every
//     SentinelAI entity (sentinel_ai.h's ApplyBeaconPerception) -- a *second*, independent
//     subscriber to the same event FlareReactorView also subscribes to for audio (Phase 5);
//     EventManager supports multiple handlers per event type, no conflict.
// This is exactly the seam ADR-0010's own VLoadLevel comment anticipated ("a future game-specific
// BaseGameLogic subclass") -- BaseGameLogic itself still knows nothing about reactors, proximity,
// beacons, or sentinels; all of that lives here (or, for the actual per-frame AI steering, in
// AIView/sentinel_ai.h -- see events.h's own header comment for why the view-side
// PlayerInteractElement does no validation of its own, same reasoning applies to perception living
// in GameLogic rather than scattered per-view).
#ifndef FLARE_REACTOR_GAME_LOGIC_H
#define FLARE_REACTOR_GAME_LOGIC_H

#include "app/view/base_game_logic.h"
#include "events.h"

class FlareReactorGameLogic : public BaseGameLogic {
public:
    FlareReactorGameLogic(entt::registry &registry, EventManager &events, ProcessManager &processes,
                           LevelLoader &levelLoader);

private:
    void OnActivateBeacon(const EvtData_ActivateBeacon &event);
    void OnBeaconTriggered(const EvtData_BeaconTriggered &event);
};

#endif // FLARE_REACTOR_GAME_LOGIC_H
