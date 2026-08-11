// DevArenaView: game/dev_arena's IGameView -- a fourth concrete view (game/sandbox's HumanView was
// the first, game/camera_fps's CameraFpsView the second, game/flare_reactor's FlareReactorView the
// third). Reuses HumanViewBase's IScreenElement stack (docs/adr/0016) exactly like the other three;
// moves the possessed actor via InputBindings (docs/adr/0013), same as FlareReactorView, not a
// hardcoded IsKeyDown scheme (game/sandbox's own HumanView predates ADR-0013 landing and hasn't
// been migrated -- this is new code, so it starts on the current convention, not the stale one).
#ifndef DEV_ARENA_HUMAN_VIEW_H
#define DEV_ARENA_HUMAN_VIEW_H

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/input/input_bindings.h"
#include "app/resource/resource_cache.h"
#include "app/scene/material.h"
#include "app/view/human_view_base.h"

class DevArenaView : public HumanViewBase {
public:
    // textures: threaded in from main.cpp (which already has Engine& in scope), not read via
    // Engine::Current() internally -- matches how FlareReactorView/HumanView (sandbox) take their
    // own ResourceCache<T>& dependencies as explicit constructor params rather than reaching for
    // the engine singleton themselves.
    DevArenaView(entt::registry &registry, ResourceCache<Texture2D> &textures);

    // Unloads material_.shader -- same reasoning game/sandbox/human_view.h's own destructor
    // comment gives (must run before Engine::Shutdown() closes the GL context this Unload* call
    // needs); dev_arena's single-scene main() (no screens.h state machine) makes this run exactly
    // once, at the very end, same as game/flare_reactor's ~FlareReactorView-equivalent lifetime.
    ~DevArenaView() override;
    DevArenaView(const DevArenaView &) = delete;
    DevArenaView &operator=(const DevArenaView &) = delete;

    void VOnUpdate(float dt) override;

private:
    entt::registry &registry_;
    Camera3D camera_;
    InputBindings input_;   // config/keybindings.yaml, loaded once here (ADR-0013 Decision 3)
    RenderMaterial material_;
};

#endif // DEV_ARENA_HUMAN_VIEW_H
