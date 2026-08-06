// The first concrete IGameView built (docs/adr/0010 Sec 4) -- game/camera_fps/human_view.h is the
// second (docs/adr/0017). Reads raylib's polled input
// directly in VOnUpdate (Decision B in that section: no Win32-style message-proc translation
// layer -- raylib has no message-queue concept to translate from, IsKeyDown/etc. already *are*
// polled per-frame input state). Composes a small IScreenElement stack for rendering
// (docs/adr/0016) instead of drawing directly -- see game/sandbox/human_view.cpp's GameplayScene/
// GameplayHud for what's actually pushed today. The IScreenElement stack plumbing itself
// (PushElement/RemoveElement, VOnRender's sort-and-dispatch, VOnAttach) lives in HumanViewBase
// (app/human_view_base.h) -- promoted there once game/camera_fps needed the exact same plumbing
// with a completely different VOnUpdate (docs/adr/0017); this class now only holds what's actually
// sandbox-specific.
#ifndef HUMAN_VIEW_H
#define HUMAN_VIEW_H

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/human_view_base.h"
#include "app/process_manager.h"
#include "app/resource_cache.h"

class HumanView : public HumanViewBase {
public:
    // processes/sounds: the "human" half of GCC4::HumanView's own dependencies (UserInterface/
    // HumanView.h -- m_pProcessManager "strictly for things like button animations, etc.",
    // InitAudio()) -- held the same way BaseGameLogic already holds a ProcessManager& it doesn't
    // call into yet (base_game_logic.h/.cpp): the seam a human-facing view needs (camera easing,
    // UI-timed effects, sound playback tied to view-level events) once one of those becomes a
    // concrete need, not before.
    HumanView(entt::registry &registry, ProcessManager &processes, ResourceCache<Sound> &sounds);

    void VOnUpdate(float dt) override;

private:
    entt::registry &registry_;
    ProcessManager &processes_;
    ResourceCache<Sound> &sounds_;
    Camera3D camera_;
};

#endif // HUMAN_VIEW_H
