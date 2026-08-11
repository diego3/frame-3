// Entry point for game/dev_arena -- a greybox test/dev scenario, not a game. Applies docs/adr/0017's
// "second game module" pattern a third time (game/camera_fps, then game/flare_reactor's own RFC,
// now this): one Engine-driven scene, no screens.h state machine (nothing here needs a Logo/Title/
// Options/Ending flow), data-driven level/entity content (assets/levels/dev_arena.yaml), BaseGameLogic
// + EntityFactory + LevelLoader wiring identical to every other game module's.
//
// Purpose (not a design decision of its own -- reuses ADR-0017/ADR-0019/ADR-0020/ADR-0013 as
// already decided, no new architecture introduced here): a reusable scale/orientation reference
// scene for visually testing new engine features (lighting, cameras, physics, whatever comes
// next) without touching game/sandbox's, game/camera_fps's, or game/flare_reactor's own real
// content. Six checkerboard-textured, differently-tinted reference cubes at known 2-unit spacing
// (assets/levels/dev_arena.yaml) plus a procedural DrawGrid give a real sense of scale -- see
// human_view.cpp's own header comment for why the floor is a grid, not a textured Renderable
// plane (GenMeshCube's UVs don't retile under a stretched scale).
#include <raylib.h>

#include <memory>
#include <optional>
#include <vector>

#include "app/core/engine.h"
#include "app/core/engine_config.h"
#include "app/entity/entity_factory.h"
#include "app/entity/entity_file_parser_yaml.h"
#include "app/entity/level_loader.h"
#include "app/scene/light.h"
#include "app/scene/renderable.h"
#include "app/scene/transform.h"
#include "app/view/base_game_logic.h"
#include "human_view.h"

namespace {
    BaseGameLogic *g_logic = nullptr;
    DevArenaView *g_view = nullptr;

    // "Position"/"Renderable" mirror game/sandbox/screen_gameplay.cpp's own loaders of the same
    // names exactly (both reuse ParseRenderableComponent, app/scene/renderable.h) -- no reason for
    // a fourth game module to define either differently. "Light" mirrors game/flare_reactor/
    // main.cpp's own loader (ParseLightComponent, app/scene/light.h) -- assets/entities/dev_arena/
    // sun.yaml is its only caller here.
    void RegisterComponentLoaders(EntityFactory &factory, Engine &engine) {
        factory.RegisterComponentLoader("Position", [](entt::registry &registry, entt::entity entity,
                                                         const EntityDefNode &node) {
            registry.emplace<LocalTransform>(
                entity, Vector3{node.Get("x").AsFloat(), node.Get("y").AsFloat(), node.Get("z").AsFloat()});
            registry.emplace<WorldTransform>(entity);
        });

        factory.RegisterComponentLoader("Renderable", [&engine](entt::registry &registry, entt::entity entity,
                                                                   const EntityDefNode &node) {
            registry.emplace<Renderable>(entity, ParseRenderableComponent(node, engine.Models(), GRAY));
        });

        factory.RegisterComponentLoader("Light", [](entt::registry &registry, entt::entity entity,
                                                      const EntityDefNode &node) {
            registry.emplace<Light>(entity, ParseLightComponent(node));
        });
    }

    void UpdateDrawFrame() {
        if (g_logic) g_logic->VOnUpdate(GetFrameTime());

        BeginDrawing();
            ClearBackground(RAYWHITE);
            if (g_view) g_view->VOnRender(GetFrameTime());
        EndDrawing();
    }
}

int main() {
    Engine engine;
    if (!engine.Init(LoadOrCreateEngineConfig(), "frame-3 dev arena (greybox test scenario)")) return 1;

    YamlEntityFileParser parser;
    EntityFactory entityFactory([](const std::string &name) {
        TraceLog(LOG_WARNING, "Unknown component '%s' in entity definition, skipping", name.c_str());
    });
    RegisterComponentLoaders(entityFactory, engine);

    LevelLoader levelLoader(entityFactory, parser);
    BaseGameLogic logic(engine.Registry(), engine.Events(), engine.Processes(), levelLoader);
    g_logic = &logic;

    // assets/levels/dev_arena.yaml lists the player first, same reason every other multi-actor
    // level in this project does (assets/levels/camera_fps.yaml) -- spawned[0] needs to be
    // unambiguous.
    std::vector<entt::entity> spawned = logic.VLoadLevel("resources/levels/dev_arena.yaml");
    std::optional<entt::entity> playerActor;
    if (!spawned.empty()) playerActor = spawned.front();

    auto view = std::make_unique<DevArenaView>(engine.Registry(), engine.Textures());
    g_view = view.get();
    logic.AttachView(std::move(view), playerActor);

    engine.Run(UpdateDrawFrame);

    g_logic = nullptr;
    g_view = nullptr;
    engine.Shutdown();
    return 0;
}
