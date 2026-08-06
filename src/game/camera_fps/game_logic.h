// Game-specific BaseGameLogic subclass -- the seam ADR-0010 Sec 2's own doc comment left open
// ("Virtual so a future game-specific BaseGameLogic subclass can add its own post-load setup")
// and docs/adr/0017 initially skipped, leaving the actual FPS body-physics simulation inside
// CameraFpsView instead. That blurred exactly the boundary ADR-0010 exists to keep: Logic owns
// simulation truth, View owns input/presentation. Moved here.
#ifndef CAMERA_FPS_GAME_LOGIC_H
#define CAMERA_FPS_GAME_LOGIC_H

#include "app/base_game_logic.h"

class CameraFpsLogic : public BaseGameLogic {
public:
    using BaseGameLogic::BaseGameLogic;

    // Advances every actor with a PlayerInput+PlayerBody+LocalTransform (components.h) one
    // physics step, *then* calls BaseGameLogic::VOnUpdate to tick attached views -- so
    // CameraFpsView renders this frame's already-integrated LocalTransform, at the cost of the
    // physics step consuming PlayerInput captured on the *previous* frame (a view only writes
    // PlayerInput once it's ticked, i.e. after this step already ran this frame). A one-frame
    // input-to-physics lag, the same tradeoff any Logic/View-separated engine accepts --
    // imperceptible at real frame rates, and self-healing: an actor with no PlayerInput yet
    // (the very first frame, before any view has run) is simply skipped, not a crash.
    void VOnUpdate(float dt) override;
};

#endif // CAMERA_FPS_GAME_LOGIC_H
