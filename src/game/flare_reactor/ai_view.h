// AIView: the flare_reactor experiment's first AI-controlled IGameView (docs/rfc/0001-flare-
// reactor-pipeline-experiment.md, Phase 6) -- GameViewType::AI (app/view/game_view.h) named since
// ADR-0010, never built until now.
//
// Deliberately thin, matching GameCode4's own AITeapotView (TeapotWarsView.h/.cpp): VOnUpdate/
// VOnRender there are empty stubs, GameView_AI bookkeeping and little else -- the real "brain"
// (FSM + state machine) lives per-actor in Lua (Assets/Scripts/ActorManager.lua's AddEnemy), not
// fused into the C++ view. This project has no scripting layer (ADR-0005 §7), so the brain lives
// in a plain C++ free function instead (sentinel_ai.h's UpdateSentinel) -- but the split is the
// same: AIView only knows *which entity*, and defers all actual behavior to a function operating
// on that entity's own components.
//
// One AIView per sentinel entity, matching the book's own 1:1 AITeapotView-per-actor cardinality
// (TeapotWars.cpp spawns one AITeapotView per AI actor, not one shared across all of them) --
// revisit only if a second sentinel makes a shared, registry-wide "AIView-as-system" (iterating
// every SentinelAI itself, ignoring possessedActor_) genuinely worth it over this.
#ifndef FLARE_REACTOR_AI_VIEW_H
#define FLARE_REACTOR_AI_VIEW_H

#include <optional>

#include <entt/entt.hpp>

#include "app/view/game_view.h"
#include "sentinel_ai.h"

class AIView : public IGameView {
public:
    explicit AIView(entt::registry &registry) : registry_(registry) {}

    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override {
        id_ = id;
        possessedActor_ = actorId;
    }

    void VOnUpdate(float dt) override {
        if (possessedActor_.has_value()) UpdateSentinel(registry_, *possessedActor_, dt);
    }

    // No-op -- the sentinel's Renderable is already drawn by FlareReactorScene's DrawRenderables
    // (human_view.cpp), which iterates every entity regardless of which view possesses it. AIView
    // doesn't own a camera or push any IScreenElement.
    void VOnRender(float dt) override { (void)dt; }

    GameViewType VGetType() const override { return GameViewType::AI; }

private:
    entt::registry &registry_;
    std::optional<entt::entity> possessedActor_;
};

#endif // FLARE_REACTOR_AI_VIEW_H
