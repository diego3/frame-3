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
#include <string>

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/events/event_manager.h"
#include "app/view/human_view_base.h"
#include "app/input/input_bindings.h"
#include "app/resource/resource_cache.h"
#include "events.h"
#include "lighting.h"
#include "skybox.h"

class FlareReactorView : public HumanViewBase {
public:
    // `skyboxCubemapPath`/`beaconSoundPath`: from GameConfig (game_config.h), not hardcoded here --
    // the caller (main.cpp) loads GameConfig once and passes these through. `lighting`: owned by
    // main.cpp (constructed before this view, so its "Renderable" component loader can call
    // Lighting::ApplyToModel while loading the level -- see main.cpp), referenced here for
    // FlareReactorScene's per-frame draw/update calls, not owned by this view.
    FlareReactorView(entt::registry &registry, EventManager &events, ResourceCache<Sound> &sounds,
                      const std::string &skyboxCubemapPath, const std::string &beaconSoundPath,
                      Lighting &lighting);

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
    // Constructed before FlareReactorScene (below) is pushed, so the scene element can hold a
    // reference to it for the whole view's lifetime -- same "member outlives the pushed element"
    // shape camera_ already has.
    Skybox skybox_;
    Lighting &lighting_;   // main.cpp-owned, see the constructor comment above
    // KEY_TAB toggles between this (gameplay: player moves, camera follows) and a raylib-builtin
    // CAMERA_FREE fly-around, added purely to make eyeballing graphics-quality changes (the skybox,
    // to start) easier -- not an InputBindings action (ADR-0013's rebindable scheme is for real
    // gameplay actions; this is a dev/validation tool, same category as debug_overlay.h's raw
    // KEY_F3 toggle, not something a player would ever rebind).
    bool freeCameraActive_ = false;
};

#endif // FLARE_REACTOR_HUMAN_VIEW_H
