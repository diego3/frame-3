// The one concrete IGameView built now (docs/adr/0010 Sec 4). Reads raylib's polled input
// directly in VOnUpdate (Decision B in that section: no Win32-style message-proc translation
// layer -- raylib has no message-queue concept to translate from, IsKeyDown/etc. already *are*
// polled per-frame input state). Composes a small IScreenElement stack for rendering
// (docs/adr/0016) instead of drawing directly -- see game/sandbox/human_view.cpp's GameplayScene/
// GameplayHud for what's actually pushed today.
#ifndef HUMAN_VIEW_H
#define HUMAN_VIEW_H

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/game_view.h"
#include "app/process_manager.h"
#include "app/resource_cache.h"
#include "app/screen_element.h"

class HumanView : public IGameView {
public:
    // processes/sounds: the "human" half of GCC4::HumanView's own dependencies (UserInterface/
    // HumanView.h -- m_pProcessManager "strictly for things like button animations, etc.",
    // InitAudio()) -- held the same way BaseGameLogic already holds a ProcessManager& it doesn't
    // call into yet (base_game_logic.h/.cpp): the seam a human-facing view needs (camera easing,
    // UI-timed effects, sound playback tied to view-level events) once one of those becomes a
    // concrete need, not before.
    HumanView(entt::registry &registry, ProcessManager &processes, ResourceCache<Sound> &sounds);

    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override;
    void VOnUpdate(float dt) override;
    void VOnRender(float dt) override;
    GameViewType VGetType() const override { return GameViewType::Human; }

    // IScreenElement stack (docs/adr/0016): lets this view compose multiple independently
    // updated/rendered/shown/hidden pieces instead of one monolithic VOnRender body. Mirrors
    // BaseGameLogic::AttachView/DetachView's id-based shape, not the book's shared_ptr<
    // IScreenElement>/std::list -- nothing here needs shared ownership.
    ScreenElementId PushElement(std::unique_ptr<IScreenElement> element);
    void RemoveElement(ScreenElementId id);

private:
    entt::registry &registry_;
    ProcessManager &processes_;
    ResourceCache<Sound> &sounds_;
    std::optional<entt::entity> possessedActor_;
    Camera3D camera_;

    std::vector<std::pair<ScreenElementId, std::unique_ptr<IScreenElement>>> elements_;
    ScreenElementId nextElementId_ = 1;
};

#endif // HUMAN_VIEW_H
