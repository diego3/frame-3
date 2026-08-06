// Second concrete IGameView (the first was game/sandbox/human_view.h) -- ported from raylib's own
// examples/core/core_3d_camera_fps.c ("raylib [core] example - 3d camera fps"). Reuses
// HumanViewBase's IScreenElement stack (docs/adr/0016) exactly like sandbox's HumanView does, but
// has a completely different VOnUpdate (WASD + mouse-look FPS body movement, no ECS actor at all --
// this game has no possessedActor_, its "player" is purely camera-attached state) -- this
// difference is what confirmed the base/subclass split was worth doing (docs/adr/0017).
#ifndef CAMERA_FPS_HUMAN_VIEW_H
#define CAMERA_FPS_HUMAN_VIEW_H

#include <raylib.h>

#include "app/human_view_base.h"

// Ported field-for-field from the example's file-local `Body` struct. Free struct (not nested in
// CameraFpsView) so game/camera_fps/human_view.cpp's FpsHud element -- which needs read-only
// access for the velocity readout -- can take one by const reference, the same way sandbox's
// GameplayScene takes an entt::registry&.
struct CameraFpsBody {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 velocity{0.0f, 0.0f, 0.0f};
    Vector3 dir{0.0f, 0.0f, 0.0f};
    bool isGrounded = false;
};

class CameraFpsView : public HumanViewBase {
public:
    CameraFpsView();

    void VOnUpdate(float dt) override;

private:
    // Remaining state ported from the example's file-local globals (lookRotation, headTimer,
    // walkLerp, headLerp, lean) -- kept as instance state here instead, since this project doesn't
    // use file-local statics for per-view state elsewhere (docs/adr/0010).
    void UpdateBody(float rot, float dt, char side, char forward, bool jumpPressed, bool crouchHold);
    void UpdateCameraFps();

    Camera3D camera_{};
    CameraFpsBody player_;
    Vector2 lookRotation_{0.0f, 0.0f};
    float headTimer_ = 0.0f;
    float walkLerp_ = 0.0f;
    float headLerp_ = 1.0f;   // STAND_HEIGHT
    Vector2 lean_{0.0f, 0.0f};
};

#endif // CAMERA_FPS_HUMAN_VIEW_H
