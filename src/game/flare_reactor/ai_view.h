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
//
// Voice lines (assets/audio/voices/, the "_limpa" mix): "just-patroling" for the sentinel's default
// state, "letsseewhatishapenning" for the moment it notices something's wrong. Loaded once per
// AIView instance via ResourceCache<Sound> (same shared-cache-of-handles the beacon SFX already
// uses in human_view.cpp), not per-play -- GetHandle() dedups by path, so a second sentinel later
// sharing the same voice line would reuse the same loaded Sound, not reload it.
#ifndef FLARE_REACTOR_AI_VIEW_H
#define FLARE_REACTOR_AI_VIEW_H

#include <memory>
#include <optional>
#include <string>

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/resource/resource_cache.h"
#include "app/view/game_view.h"
#include "sentinel_ai.h"

class AIView : public IGameView {
public:
    // `patrolVoicePath`/`investigateVoicePath`: from GameConfig (game_config.h), not hardcoded --
    // the caller (main.cpp) loads GameConfig once and passes these through.
    AIView(entt::registry &registry, ResourceCache<Sound> &sounds, const std::string &patrolVoicePath,
           const std::string &investigateVoicePath)
        : registry_(registry), sounds_(sounds) {
        patrolVoice_ = sounds_.GetHandle(patrolVoicePath);
        investigateVoice_ = sounds_.GetHandle(investigateVoicePath);
    }

    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override {
        id_ = id;
        possessedActor_ = actorId;

        // SentinelAI always starts in SentinelState::Patrol (sentinel_ai.h's own struct default,
        // never authored per-instance -- see main.cpp's "SentinelAI" component loader), so this
        // moment -- the view actually taking possession of a patrolling sentinel -- is this voice
        // line's one and only trigger, matching the FSM's own lack of a "return to patrol"
        // transition (sentinel_ai.h's comment on that) that would otherwise be a second one.
        if (possessedActor_.has_value() && patrolVoice_) {
            TraceLog(LOG_INFO, "AIView: sentinel attached in Patrol -- playing just-patroling_limpa.mp3");
            PlaySound(*patrolVoice_);
        }
    }

    void VOnUpdate(float dt) override {
        if (!possessedActor_.has_value()) return;

        // Fires exactly once, on the Patrol -> Investigate transition (the effect
        // ApplyBeaconPerception, sentinel_ai.h, has on this actor once BeaconTriggered's hearing-
        // radius check lets it through) -- detected by diffing state across frames here rather
        // than this view subscribing to EvtData_BeaconTriggered itself, so it doesn't have to
        // duplicate ApplyBeaconPerception's own hearing-radius gate (this sentinel might be too far
        // to react at all) or reason about dispatch order against FlareReactorGameLogic's own
        // subscriber for the same event.
        if (const SentinelAI *ai = registry_.try_get<SentinelAI>(*possessedActor_)) {
            if (ai->state == SentinelState::Investigate && lastState_ == SentinelState::Patrol &&
                investigateVoice_) {
                TraceLog(LOG_INFO, "AIView: sentinel Patrol -> Investigate -- playing "
                                    "letsseewhatishapenning_limpa.mp3");
                PlaySound(*investigateVoice_);
            }
            lastState_ = ai->state;
        }

        UpdateSentinel(registry_, *possessedActor_, dt);
    }

    // No-op -- the sentinel's Renderable is already drawn by FlareReactorScene's DrawRenderables
    // (human_view.cpp), which iterates every entity regardless of which view possesses it. AIView
    // doesn't own a camera or push any IScreenElement.
    void VOnRender(float dt) override { (void)dt; }

    GameViewType VGetType() const override { return GameViewType::AI; }

private:
    entt::registry &registry_;
    ResourceCache<Sound> &sounds_;
    std::shared_ptr<Sound> patrolVoice_;
    std::shared_ptr<Sound> investigateVoice_;
    std::optional<entt::entity> possessedActor_;
    SentinelState lastState_ = SentinelState::Patrol;
};

#endif // FLARE_REACTOR_AI_VIEW_H
