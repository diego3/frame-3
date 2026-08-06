// Second concrete IGameView (the first was game/sandbox/human_view.h) -- ported from raylib's own
// examples/core/core_3d_camera_fps.c ("raylib [core] example - 3d camera fps"). Reuses
// HumanViewBase's IScreenElement stack (docs/adr/0016) exactly like sandbox's HumanView does.
//
// This class itself holds no per-frame logic at all: every frame's actual work (reading input,
// publishing MovementIntent, easing the camera, rendering, the F3 overlay) lives in the
// IScreenElements pushed in the constructor (human_view.cpp) -- CameraFpsView doesn't even
// override VOnUpdate, relying on HumanViewBase's default (just ticks those elements). Its own two
// jobs are composition (the constructor) and VOnAttach, which seeds FirstPersonCameraRig onto the
// newly-possessed actor -- a lifecycle hook only the view itself has, not something an
// IScreenElement participates in.
#ifndef CAMERA_FPS_HUMAN_VIEW_H
#define CAMERA_FPS_HUMAN_VIEW_H

#include <entt/entt.hpp>

#include "app/human_view_base.h"

class CameraFpsView : public HumanViewBase {
public:
    explicit CameraFpsView(entt::registry &registry);

    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override;

private:
    entt::registry &registry_;
};

#endif // CAMERA_FPS_HUMAN_VIEW_H
