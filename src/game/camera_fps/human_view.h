// Second concrete IGameView (the first was game/sandbox/human_view.h) -- ported from raylib's own
// examples/core/core_3d_camera_fps.c ("raylib [core] example - 3d camera fps"). Reuses
// HumanViewBase's IScreenElement stack (docs/adr/0016) exactly like sandbox's HumanView does. The
// possessed actor's PlayerBody + LocalTransform (both real ECS components -- docs/adr/0017) hold
// the movement simulation's actual data; this class only holds view-local presentation state
// (Camera3D, look/head-bob easing) that has no meaning outside "how the camera currently looks",
// the same split game/sandbox/human_view.h already draws between its own camera_ (view-local) and
// the possessed actor's LocalTransform (world truth).
#ifndef CAMERA_FPS_HUMAN_VIEW_H
#define CAMERA_FPS_HUMAN_VIEW_H

#include <raylib.h>
#include <entt/entt.hpp>

#include "app/human_view_base.h"

class CameraFpsView : public HumanViewBase {
public:
    explicit CameraFpsView(entt::registry &registry);

    void VOnUpdate(float dt) override;

private:
    // Remaining state ported from the example's file-local globals (lookRotation, headTimer,
    // walkLerp, headLerp, lean) -- view-local camera easing, not simulation state, so it stays
    // here rather than becoming a component (docs/adr/0010).
    void UpdateBody(float rot, float dt, char side, char forward, bool jumpPressed, bool crouchHold);
    void UpdateCameraFps();

    entt::registry &registry_;
    Camera3D camera_{};
    Vector2 lookRotation_{0.0f, 0.0f};
    float headTimer_ = 0.0f;
    float walkLerp_ = 0.0f;
    float headLerp_ = 1.0f;   // STAND_HEIGHT
    Vector2 lean_{0.0f, 0.0f};
};

#endif // CAMERA_FPS_HUMAN_VIEW_H
