// Entry point for game/camera_fps -- raylib's examples/core/core_3d_camera_fps.c ("raylib [core]
// example - 3d camera fps") ported in as frame-3's second concrete game module (docs/adr/0017).
// No screens.h-style multi-screen state machine here (the original example has none either --
// game/sandbox's Logo/Title/Options/Ending machinery is sandbox content, not part of the
// IGameView/IScreenElement pattern being reused): this is one Engine-driven scene for the whole
// run. No GameConfig -- the example uses no assets. Level/entity content IS data-driven, same
// wiring game/sandbox/screen_gameplay.cpp uses (BaseGameLogic + EntityFactory + LevelLoader +
// assets/levels/camera_fps.yaml) -- the player and its 4 towers are real actors, not hardcoded
// draw calls (see human_view.cpp for what's still legitimately just scene dressing: floor + sun).

#include <raylib.h>

#include <memory>
#include <optional>
#include <vector>

#include "app/base_game_logic.h"
#include "app/debug_overlay.h"
#include "app/engine.h"
#include "app/engine_config.h"
#include "app/entity_factory.h"
#include "app/entity_file_parser_yaml.h"
#include "app/level_loader.h"
#include "app/render_components.h"
#include "app/transform.h"
#include "components.h"
#include "human_view.h"

namespace {
    BaseGameLogic *g_logic = nullptr;
    CameraFpsView *g_view = nullptr;

    // "Position" mirrors game/sandbox/screen_gameplay.cpp's own loader exactly (both spawn an
    // entity with a LocalTransform/WorldTransform at a given point -- no reason for the two games
    // to define this differently). "BoxRenderable" is new here -- assets/entities/tower.yaml is
    // its only caller today.
    void RegisterComponentLoaders(EntityFactory &factory) {
        factory.RegisterComponentLoader("Position", [](entt::registry &registry, entt::entity entity,
                                                         const EntityDefNode &node) {
            registry.emplace<LocalTransform>(
                entity, Vector3{node.Get("x").AsFloat(), node.Get("y").AsFloat(), node.Get("z").AsFloat()});
            registry.emplace<WorldTransform>(entity);
        });

        factory.RegisterComponentLoader("BoxRenderable", [](entt::registry &registry, entt::entity entity,
                                                              const EntityDefNode &node) {
            const EntityDefNode &size = node.Get("size");
            const EntityDefNode &color = node.Get("color");
            registry.emplace<BoxRenderable>(
                entity,
                Vector3{size.Get("x").AsFloat(), size.Get("y").AsFloat(), size.Get("z").AsFloat()},
                Color{static_cast<unsigned char>(color.Get("r").AsInt()),
                      static_cast<unsigned char>(color.Get("g").AsInt()),
                      static_cast<unsigned char>(color.Get("b").AsInt()),
                      static_cast<unsigned char>(color.Get("a").AsInt(255))});
        });
    }

    void UpdateDrawFrame() {
        UpdateDebugOverlay(GetFrameTime());   // F3 toggles a /proc/self stats HUD (Linux desktop only)

        if (g_logic) g_logic->VOnUpdate(GetFrameTime());

        BeginDrawing();
            ClearBackground(RAYWHITE);
            if (g_view) g_view->VOnRender(GetFrameTime());
            DrawDebugOverlay();
        EndDrawing();
    }
}

int main() {
    Engine engine;
    if (!engine.Init(LoadOrCreateEngineConfig(), "raylib [core] example - 3d camera fps")) return 1;

    DisableCursor();   // Limit cursor to relative movement inside the window (mouse-look)

    YamlEntityFileParser parser;
    EntityFactory entityFactory([](const std::string &name) {
        TraceLog(LOG_WARNING, "Unknown component '%s' in entity definition, skipping", name.c_str());
    });
    RegisterComponentLoaders(entityFactory);

    LevelLoader levelLoader(entityFactory, parser);
    BaseGameLogic logic(engine.Registry(), engine.Events(), engine.Processes(), levelLoader);
    g_logic = &logic;

    // assets/levels/camera_fps.yaml lists the player first specifically so spawned[0] below is
    // unambiguous (docs/adr/0017 -- the first level in this project with more than one entity).
    std::vector<entt::entity> spawned = logic.VLoadLevel("resources/levels/camera_fps.yaml");
    std::optional<entt::entity> playerActor;
    if (!spawned.empty()) {
        playerActor = spawned.front();
        engine.Registry().emplace<PlayerBody>(*playerActor);
    }

    auto view = std::make_unique<CameraFpsView>(engine.Registry());
    g_view = view.get();
    logic.AttachView(std::move(view), playerActor);

    engine.Run(UpdateDrawFrame);

    g_logic = nullptr;
    g_view = nullptr;
    engine.Shutdown();
    return 0;
}
