// Second concrete IGameView (the first was game/sandbox/human_view.h) -- ported from raylib's own
// examples/core/core_3d_camera_fps.c ("raylib [core] example - 3d camera fps"). Reuses
// HumanViewBase's IScreenElement stack (docs/adr/0016) exactly like sandbox's HumanView does.
// Unlike sandbox's HumanView, this class holds no per-frame simulation OR presentation state of
// its own at all -- the possessed actor's PlayerBody, LocalTransform, and FirstPersonCameraRig
// (all real ECS components, game/camera_fps/components.h) hold everything, including the camera
// itself and its look/head-bob easing. VOnAttach seeds FirstPersonCameraRig onto the actor;
// VOnUpdate reads/writes it. See components.h for why nothing here may cache a pointer into it
// across frames.
#ifndef CAMERA_FPS_HUMAN_VIEW_H
#define CAMERA_FPS_HUMAN_VIEW_H

#include <entt/entt.hpp>

#include "app/human_view_base.h"

class CameraFpsView : public HumanViewBase {
public:
    explicit CameraFpsView(entt::registry &registry);

    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override;
    void VOnUpdate(float dt) override;

private:
    entt::registry &registry_;
};

#endif // CAMERA_FPS_HUMAN_VIEW_H
