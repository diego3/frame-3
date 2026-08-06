// Generic half of a "human" IGameView (docs/adr/0010 Sec 4), promoted out of
// game/sandbox/human_view.h/.cpp into app/ once a second concrete game (game/camera_fps) needed
// the exact same plumbing -- the trigger docs/adr/0015 said to wait for before generalizing
// anything (docs/adr/0017). Owns the IScreenElement stack (docs/adr/0016): push/remove elements,
// the sorted VOnRender dispatch, and VOnAttach's id_/possessedActor_ bookkeeping. Deliberately does
// NOT implement VOnUpdate -- reading input and deciding what a "human" does with it is exactly the
// part that differs per game (sandbox's arrow-key box nudging vs. camera_fps's WASD+mouse FPS
// movement), left abstract for each game's own subclass.
#ifndef HUMAN_VIEW_BASE_H
#define HUMAN_VIEW_BASE_H

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entt.hpp>

#include "game_view.h"
#include "screen_element.h"

class HumanViewBase : public IGameView {
public:
    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override;
    void VOnRender(float dt) override;
    GameViewType VGetType() const override { return GameViewType::Human; }

    // Lets a subclass compose multiple independently updated/rendered/shown/hidden pieces instead
    // of one monolithic VOnRender body. Mirrors BaseGameLogic::AttachView/DetachView's id-based
    // shape, not the book's shared_ptr<IScreenElement>/std::list -- nothing here needs shared
    // ownership.
    ScreenElementId PushElement(std::unique_ptr<IScreenElement> element);
    void RemoveElement(ScreenElementId id);

protected:
    // Not every subclass has an ECS actor to possess -- game/sandbox/human_view.cpp reads this to
    // move a LocalTransform; game/camera_fps/human_view.cpp never sets it (its "player" is
    // camera-attached state, not an entity).
    std::optional<entt::entity> possessedActor_;

    // Ticks every pushed element's VOnUpdate -- a subclass's own VOnUpdate override calls this
    // itself (not run implicitly) so it controls ordering against its own input handling, the same
    // way game/sandbox/human_view.cpp's VOnUpdate already ran this before reading movement keys.
    void UpdateElements(float dt);

private:
    std::vector<std::pair<ScreenElementId, std::unique_ptr<IScreenElement>>> elements_;
    ScreenElementId nextElementId_ = 1;
};

#endif // HUMAN_VIEW_BASE_H
