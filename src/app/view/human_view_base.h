// Generic half of a "human" IGameView (docs/adr/0010 Sec 4), promoted out of
// game/sandbox/human_view.h/.cpp into app/ once a second concrete game (game/camera_fps) needed
// the exact same plumbing -- the trigger docs/adr/0015 said to wait for before generalizing
// anything (docs/adr/0017). Owns the IScreenElement stack (docs/adr/0016): push/remove elements,
// the sorted VOnRender dispatch, and VOnAttach's id_/possessedActor_ bookkeeping.
#ifndef HUMAN_VIEW_BASE_H
#define HUMAN_VIEW_BASE_H

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entt.hpp>

#include "app/view/game_view.h"
#include "app/view/screen_element.h"

class HumanViewBase : public IGameView {
public:
    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override;
    void VOnRender(float dt) override;
    GameViewType VGetType() const override { return GameViewType::Human; }

    // Default: just ticks the pushed element stack. Non-pure (unlike VOnRender being the only
    // other thing every subclass needs) specifically so a subclass whose entire "human" update is
    // element dispatch -- game/camera_fps/human_view.cpp's CameraFpsView, once its own per-frame
    // input/camera work moved into a pushed PlayerMovementElement (docs/adr/0017 follow-up) --
    // doesn't need to override this at all. A subclass that needs more (game/sandbox's HumanView,
    // which also moves its possessed actor directly) still overrides it, calling UpdateElements
    // itself to control ordering against its own input handling.
    void VOnUpdate(float dt) override;

    // Lets a subclass compose multiple independently updated/rendered/shown/hidden pieces instead
    // of one monolithic VOnRender body. Mirrors BaseGameLogic::AttachView/DetachView's id-based
    // shape, not the book's shared_ptr<IScreenElement>/std::list -- nothing here needs shared
    // ownership.
    ScreenElementId PushElement(std::unique_ptr<IScreenElement> element);
    void RemoveElement(ScreenElementId id);

protected:
    // Not every subclass has an ECS actor to possess -- both game/sandbox/human_view.cpp and
    // game/camera_fps/human_view.cpp set it today, but a RemoteView/observer-only view (still
    // unbuilt) legitimately wouldn't.
    std::optional<entt::entity> possessedActor_;

    void UpdateElements(float dt);

private:
    std::vector<std::pair<ScreenElementId, std::unique_ptr<IScreenElement>>> elements_;
    ScreenElementId nextElementId_ = 1;
};

#endif // HUMAN_VIEW_BASE_H
