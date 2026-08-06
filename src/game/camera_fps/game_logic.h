// Game-specific BaseGameLogic subclass -- the seam ADR-0010 Sec 2's own doc comment left open
// ("Virtual so a future game-specific BaseGameLogic subclass can add its own post-load setup")
// and docs/adr/0017 initially skipped, leaving the actual FPS body-physics simulation inside
// CameraFpsView instead. That blurred exactly the boundary ADR-0010 exists to keep: Logic owns
// simulation truth, View owns input/presentation. Moved here.
#ifndef CAMERA_FPS_GAME_LOGIC_H
#define CAMERA_FPS_GAME_LOGIC_H

#include <entt/entt.hpp>

#include "app/base_game_logic.h"

// Fired whenever the physics step below triggers a jump. The ported example's own UpdateBody()
// had a commented-out "Sound can be played at this moment" hook (SetSoundPitch/PlaySound) at
// exactly this point in the original raylib example -- this is that hook, done the way this
// project's own precedent already does it (app/level_loader.h's EvtData_EntitySpawned, ADR-0009's
// "View-plurality seam, kept on purpose"): queued via EventManager (docs/adr/0005) even though
// nothing subscribes yet, so CameraFpsLogic never has to change once a future View/audio system
// wants to react to it.
struct EvtData_ActorJumped {
    entt::entity entity;
};

class CameraFpsLogic : public BaseGameLogic {
public:
    using BaseGameLogic::BaseGameLogic;

    // Advances every actor with a MovementIntent+PlayerBody+LocalTransform (components.h) one
    // physics step, *then* calls BaseGameLogic::VOnUpdate to tick attached views -- so
    // CameraFpsView renders this frame's already-integrated LocalTransform, at the cost of the
    // physics step consuming MovementIntent captured on the *previous* frame (a view only writes
    // it once it's ticked, i.e. after this step already ran this frame). A one-frame
    // input-to-physics lag, the same tradeoff any Logic/View-separated engine accepts --
    // imperceptible at real frame rates, and self-healing: an actor with no MovementIntent yet
    // (the very first frame, before any view has run) is simply skipped, not a crash.
    //
    // registry_.view<...>() below IS this project's ECS-native form of "GameLogic walks its
    // actors, updating their components" -- there's no separate actor list to iterate by hand the
    // way an OOP Actor-with-components model would need; the view itself is that walk, scoped to
    // exactly the actors that have every component this one system cares about. A second system
    // (e.g. ADR-0012's eventual physics, or an AI decision step) would be another `registry_.view`
    // over its own component signature, not a shared "for each actor" loop -- systems don't share
    // a traversal in this model, each just declares which components it needs.
    void VOnUpdate(float dt) override;
};

#endif // CAMERA_FPS_GAME_LOGIC_H
