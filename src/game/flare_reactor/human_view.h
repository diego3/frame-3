// FlareReactorView: the flare_reactor experiment's IGameView (docs/rfc/0001-flare-reactor-
// pipeline-experiment.md). Renders every entity's Renderable (app/scene/renderable.h) and moves
// the possessed (PlayerTag) actor -- through InputBindings (docs/adr/0013), not a hardcoded
// IsKeyDown scheme; this is the first real (non-test) caller of InputBindings in the product.
// Interact (KEY_E by default) is handled by PlayerInteractElement (human_view.cpp), pushed once
// VOnAttach knows the possessed actor -- it only translates the key into EvtData_ActivateBeacon
// (events.h), no validation of its own; FlareReactorGameLogic (Phase 3) owns that.
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

#include "app/events/event_manager.h"
#include "app/view/game_view.h"
#include "app/input/input_bindings.h"
#include "app/view/screen_element.h"

class FlareReactorView : public IGameView {
public:
    FlareReactorView(entt::registry &registry, EventManager &events);

    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override;
    void VOnUpdate(float dt) override;
    void VOnRender(float dt) override;
    GameViewType VGetType() const override { return GameViewType::Human; }

    ScreenElementId PushElement(std::unique_ptr<IScreenElement> element);
    void RemoveElement(ScreenElementId id);

private:
    entt::registry &registry_;
    EventManager &events_;
    std::optional<entt::entity> possessedActor_;
    Camera3D camera_;
    InputBindings input_;   // config/keybindings.yaml, loaded once here (ADR-0013 Decision 3)

    std::vector<std::pair<ScreenElementId, std::unique_ptr<IScreenElement>>> elements_;
    ScreenElementId nextElementId_ = 1;
};

#endif // FLARE_REACTOR_HUMAN_VIEW_H
