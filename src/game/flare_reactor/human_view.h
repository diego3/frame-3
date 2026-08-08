// FlareReactorView: the flare_reactor experiment's IGameView (docs/rfc/0001-flare-reactor-
// pipeline-experiment.md). Renders every entity's Renderable (app/scene/renderable.h) and moves
// the possessed (PlayerTag) actor -- through InputBindings (docs/adr/0013), not a hardcoded
// IsKeyDown scheme; this is the first real (non-test) caller of InputBindings in the product.
// Interact (KEY_E by default) is handled by PlayerInteractElement (human_view.cpp), pushed once
// VOnAttach knows the possessed actor -- it only translates the key into EvtData_ActivateBeacon
// (events.h), no validation of its own; FlareReactorGameLogic (Phase 3) owns that.
//
// Also this project's first real (non-dead) use of a view directly holding a
// ResourceCache<Sound>& (Phase 5) -- game/sandbox's own HumanView holds one too, but nothing
// calls into it (screens.h's PlaySound(fxCoin) calls go through a plain global instead). Here,
// OnBeaconTriggered (subscribed to EvtData_BeaconTriggered in the constructor) actually uses it.
//
// Used to duplicate game/sandbox/HumanView's small IScreenElement-stack plumbing
// (PushElement/RemoveElement, sorted VOnRender) rather than sharing it, since there was no
// HumanViewBase in this tree yet to subclass -- promoted out to app/view/human_view_base.h/.cpp by
// the claude/camera-fps-second-game-module branch (docs/adr/0017), and this view rebased onto it
// once that branch merged, dropping its own copy of the same plumbing (VOnAttach/VOnRender/
// PushElement/RemoveElement all now inherited).
#ifndef FLARE_REACTOR_HUMAN_VIEW_H
#define FLARE_REACTOR_HUMAN_VIEW_H

#include <memory>
#include <optional>

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/events/event_manager.h"
#include "app/view/human_view_base.h"
#include "app/input/input_bindings.h"
#include "app/resource/resource_cache.h"
#include "events.h"

class FlareReactorView : public HumanViewBase {
public:
    FlareReactorView(entt::registry &registry, EventManager &events, ResourceCache<Sound> &sounds);

    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override;
    void VOnUpdate(float dt) override;

private:
    void OnBeaconTriggered(const EvtData_BeaconTriggered &event);

    entt::registry &registry_;
    EventManager &events_;
    ResourceCache<Sound> &sounds_;
    std::shared_ptr<Sound> beaconSound_;   // loaded once in the constructor, released before Shutdown
    Camera3D camera_;
    InputBindings input_;   // config/keybindings.yaml, loaded once here (ADR-0013 Decision 3)
};

#endif // FLARE_REACTOR_HUMAN_VIEW_H
