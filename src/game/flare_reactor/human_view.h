// FlareReactorView: the flare_reactor experiment's IGameView (docs/rfc/0001-flare-reactor-
// pipeline-experiment.md). Phase 1 scope only: renders every entity's Renderable (app/renderable.h)
// and moves the possessed (PlayerTag) actor with the same placeholder arrow-key scheme
// game/sandbox/human_view.cpp already uses -- no interact key, no ProcessManager/audio/AI
// reactions yet (later phases).
//
// Deliberately duplicates game/sandbox/HumanView's small IScreenElement-stack plumbing
// (PushElement/RemoveElement, sorted VOnRender) rather than sharing it -- there is no
// HumanViewBase in this tree to subclass (main doesn't have the claude/camera-fps-second-game-
// module branch's app/human_view_base.h/.cpp yet). If that branch merges first, this should be
// rebased onto whatever it ships instead of keeping its own copy of the same plumbing.
#ifndef FLARE_REACTOR_HUMAN_VIEW_H
#define FLARE_REACTOR_HUMAN_VIEW_H

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/game_view.h"
#include "app/screen_element.h"

class FlareReactorView : public IGameView {
public:
    explicit FlareReactorView(entt::registry &registry);

    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override;
    void VOnUpdate(float dt) override;
    void VOnRender(float dt) override;
    GameViewType VGetType() const override { return GameViewType::Human; }

    ScreenElementId PushElement(std::unique_ptr<IScreenElement> element);
    void RemoveElement(ScreenElementId id);

private:
    entt::registry &registry_;
    std::optional<entt::entity> possessedActor_;
    Camera3D camera_;

    std::vector<std::pair<ScreenElementId, std::unique_ptr<IScreenElement>>> elements_;
    ScreenElementId nextElementId_ = 1;
};

#endif // FLARE_REACTOR_HUMAN_VIEW_H
