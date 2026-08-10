// The first concrete IGameView built (docs/adr/0010 Sec 4) -- game/camera_fps/human_view.h is the
// second (docs/adr/0017). Reads raylib's polled input
// directly in VOnUpdate (Decision B in that section: no Win32-style message-proc translation
// layer -- raylib has no message-queue concept to translate from, IsKeyDown/etc. already *are*
// polled per-frame input state). Composes a small IScreenElement stack for rendering
// (docs/adr/0016) instead of drawing directly -- see game/sandbox/human_view.cpp's GameplayScene/
// GameplayHud for what's actually pushed today. The IScreenElement stack plumbing itself
// (PushElement/RemoveElement, VOnRender's sort-and-dispatch, VOnAttach) lives in HumanViewBase
// (app/view/human_view_base.h) -- promoted there once game/camera_fps needed the exact same plumbing
// with a completely different VOnUpdate (docs/adr/0017); this class now only holds what's actually
// sandbox-specific.
#ifndef HUMAN_VIEW_H
#define HUMAN_VIEW_H

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/scene/material.h"
#include "app/view/human_view_base.h"
#include "app/process/process_manager.h"
#include "app/resource/resource_cache.h"

class HumanView : public HumanViewBase {
public:
    // processes/sounds: the "human" half of GCC4::HumanView's own dependencies (UserInterface/
    // HumanView.h -- m_pProcessManager "strictly for things like button animations, etc.",
    // InitAudio()) -- held the same way BaseGameLogic already holds a ProcessManager& it doesn't
    // call into yet (base_game_logic.h/.cpp): the seam a human-facing view needs (camera easing,
    // UI-timed effects, sound playback tied to view-level events) once one of those becomes a
    // concrete need, not before.
    HumanView(entt::registry &registry, ProcessManager &processes, ResourceCache<Sound> &sounds);

    // Unloads material_.shader (see the constructor's comment on why sandbox owns this shader
    // itself rather than through a Lighting-style class) -- needed here, not left to process exit,
    // because GAMEPLAY can be Init/Unload'd repeatedly across a single run (main.cpp's screen state
    // machine), unlike game/flare_reactor's single-shot main().
    ~HumanView() override;
    HumanView(const HumanView &) = delete;
    HumanView &operator=(const HumanView &) = delete;

    void VOnUpdate(float dt) override;

private:
    entt::registry &registry_;
    ProcessManager &processes_;
    ResourceCache<Sound> &sounds_;
    Camera3D camera_;
    // ADR-0019's Material/Renderer layer, sandbox's own instance -- deliberately built independently
    // of game/flare_reactor/lighting.h's Lighting class (not shared, not reused as-is) so this
    // actually exercises app/scene/material.h's RenderMaterial/ApplyExtras API as a second,
    // independently-built consumer, the validation ADR-0019 was written around, rather than just
    // wrapping the reactor's own class. Same rim-glow extras technique (rimColor/rimPower/
    // rimIntensity, resources/shaders/glsl330/lighting.vs/.fs), different tuning -- see
    // human_view.cpp's kRimColor et al.
    RenderMaterial material_;
};

#endif // HUMAN_VIEW_H
