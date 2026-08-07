// FlareReactorGameLogic: this experiment's first game-specific BaseGameLogic subclass
// (docs/rfc/0001-flare-reactor-pipeline-experiment.md, Phase 3 -- resolves the "onde mora o
// Subscribe de GameLogic" question the RFC's own "Questões em aberto" left open). Constructor
// subscribes the one rule this experiment has so far: validating EvtData_ActivateBeacon
// (proximity + reactor not already active) before granting it as EvtData_BeaconTriggered. This is
// exactly the seam ADR-0010's own VLoadLevel comment anticipated ("a future game-specific
// BaseGameLogic subclass") -- BaseGameLogic itself still knows nothing about reactors, proximity,
// or beacons; all of that lives here, not in the view (see events.h's own header comment for why
// the view-side PlayerInteractElement does no validation of its own).
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
};

#endif // FLARE_REACTOR_GAME_LOGIC_H
