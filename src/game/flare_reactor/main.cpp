// flare_reactor: third concrete game module (docs/adr/0014's src/game/<game-id>/ boundary),
// scaffolding the experiment designed in docs/rfc/0001-flare-reactor-pipeline-experiment.md.
// Phase 1 ("esqueleto") only: loads three tagged, data-driven entities (player/reactor/sentinel)
// via EntityFactory/LevelLoader (ADR-0008/0009) and renders each one's Renderable
// (app/renderable.h) -- proves entities with distinct shape/color render from data instead of a
// hardcoded DrawCubeWires. No interact key, no ProcessManager pulse, no audio, no AI reaction yet
// -- later phases.
//
// No screens.h state machine -- a single Engine-driven scene, same shape the RFC's own "Fases
// propostas" section describes and the (unmerged) claude/camera-fps-second-game-module branch's
// game/camera_fps/main.cpp already established as this project's pattern for a focused,
// non-menu-driven game module.
#include "raylib.h"

#include <memory>
#include <optional>
#include <string>

#include <entt/entt.hpp>

#include "app/view/base_game_logic.h"
#include "app/view/debug_overlay.h"
#include "app/core/engine.h"
#include "app/core/engine_config.h"
#include "app/entity/entity_factory.h"
#include "app/entity/entity_file_parser_yaml.h"
#include "app/entity/level_loader.h"
#include "app/scene/renderable.h"
#include "app/scene/transform.h"
#include "ai_view.h"
#include "game_logic.h"
#include "human_view.h"
#include "reactor.h"
#include "sentinel_ai.h"
#include "tags.h"

namespace {
    Color ParseColorName(const std::string &name, Color fallback) {
        if (name == "red") return RED;
        if (name == "blue") return BLUE;
        if (name == "darkgray" || name == "dark_gray") return DARKGRAY;
        if (name == "green") return GREEN;
        if (name == "orange") return ORANGE;
        if (name == "maroon") return MAROON;
        if (name == "gray" || name == "grey") return GRAY;
        return fallback;
    }

    // Mirrors game/sandbox/screen_gameplay.cpp's RegisterComponentLoaders shape exactly (same
    // EntityFactory API, same "one lambda per component name" pattern) -- not shared between the
    // two modules because "Position" is the only loader they'd actually have in common, and
    // duplicating one eight-line lambda is cheaper than a shared registration helper nobody else
    // needs yet.
    void RegisterComponentLoaders(EntityFactory &factory) {
        factory.RegisterComponentLoader("Position", [](entt::registry &registry, entt::entity entity,
                                                         const EntityDefNode &node) {
            registry.emplace<LocalTransform>(
                entity, Vector3{node.Get("x").AsFloat(), node.Get("y").AsFloat(), node.Get("z").AsFloat()});
            registry.emplace<WorldTransform>(entity);
        });

        factory.RegisterComponentLoader("PlayerTag", [](entt::registry &registry, entt::entity entity,
                                                          const EntityDefNode &) {
            registry.emplace<PlayerTag>(entity);
        });
        // Reactor (Phase 3): replaces the old ReactorTag marker -- component presence is now this
        // entity's identity (see reactor.h). Value is a bare bool in YAML ("Reactor: false"), same
        // shape as PlayerTag's "Tag: true", just not discarded here since Reactor actually carries
        // state.
        factory.RegisterComponentLoader("Reactor", [](entt::registry &registry, entt::entity entity,
                                                        const EntityDefNode &node) {
            registry.emplace<Reactor>(entity, Reactor{node.AsBool(false)});
        });
        // SentinelAI (Phase 6): replaces the old SentinelTag marker, same reasoning as Reactor
        // above. YAML value is discarded -- initial state (Patrol, zero velocity) always comes
        // from SentinelAI's own struct defaults, never authored per-instance.
        factory.RegisterComponentLoader("SentinelAI", [](entt::registry &registry, entt::entity entity,
                                                           const EntityDefNode &) {
            registry.emplace<SentinelAI>(entity);
        });
        // Patrol (Phase 6): a list of world-space waypoints, block-style only (mini-yaml
        // limitation, docs/adr/0008 -- same reason player.yaml's own Position is block-style).
        factory.RegisterComponentLoader("Patrol", [](entt::registry &registry, entt::entity entity,
                                                       const EntityDefNode &node) {
            Patrol patrol;
            if (const EntityDefNode *waypoints = node.TryGet("waypoints")) {
                for (const EntityDefNode &waypoint : waypoints->AsList()) {
                    patrol.waypoints.push_back(Vector3{waypoint.Get("x").AsFloat(),
                                                        waypoint.Get("y").AsFloat(),
                                                        waypoint.Get("z").AsFloat()});
                }
            }
            registry.emplace<Patrol>(entity, patrol);
        });

        factory.RegisterComponentLoader("Renderable", [](entt::registry &registry, entt::entity entity,
                                                           const EntityDefNode &node) {
            Renderable renderable;
            if (const EntityDefNode *shape = node.TryGet("shape")) {
                renderable.shape = (shape->AsString("box") == "sphere") ? Renderable::Shape::Sphere
                                                                         : Renderable::Shape::Box;
            }
            if (const EntityDefNode *size = node.TryGet("size")) {
                renderable.size = Vector3{size->Get("x").AsFloat(1.0f), size->Get("y").AsFloat(1.0f),
                                           size->Get("z").AsFloat(1.0f)};
            }
            if (const EntityDefNode *color = node.TryGet("color")) {
                renderable.color = ParseColorName(color->AsString("gray"), GRAY);
            }
            if (const EntityDefNode *wireframe = node.TryGet("wireframe")) {
                renderable.wireframe = wireframe->AsBool(true);
            }
            registry.emplace<Renderable>(entity, renderable);
        });
    }

    YamlEntityFileParser g_parser;
    std::unique_ptr<EntityFactory> g_entityFactory;
    std::unique_ptr<LevelLoader> g_levelLoader;
    std::unique_ptr<BaseGameLogic> g_logic;
    FlareReactorView *g_view = nullptr;   // non-owning -- g_logic owns it via AttachView

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
    if (!engine.Init(LoadOrCreateEngineConfig(), "flare reactor experiment")) return 1;

    g_entityFactory = std::make_unique<EntityFactory>([](const std::string &name) {
        TraceLog(LOG_WARNING, "Unknown component '%s' in entity definition, skipping", name.c_str());
    });
    RegisterComponentLoaders(*g_entityFactory);

    g_levelLoader = std::make_unique<LevelLoader>(*g_entityFactory, g_parser);
    // FlareReactorGameLogic (Phase 3), not plain BaseGameLogic -- its constructor subscribes the
    // EvtData_ActivateBeacon validation handler. g_logic's declared type stays BaseGameLogic*
    // (below) since main.cpp itself never needs anything FlareReactorGameLogic-specific.
    g_logic = std::make_unique<FlareReactorGameLogic>(engine.Registry(), engine.Events(),
                                                        engine.Processes(), *g_levelLoader);
    g_logic->VLoadLevel("resources/levels/flare_reactor.yaml");

    auto view = std::make_unique<FlareReactorView>(engine.Registry(), engine.Events(), engine.Sounds());
    g_view = view.get();

    // PlayerTag resolves this explicitly instead of game/sandbox's "first entity in the registry"
    // heuristic -- see tags.h.
    std::optional<entt::entity> playerActor;
    auto players = engine.Registry().view<PlayerTag>();
    if (players.begin() != players.end()) playerActor = *players.begin();

    g_logic->AttachView(std::move(view), playerActor);

    // AIView (Phase 6): one per sentinel entity, resolved via SentinelAI's own presence (same
    // component-is-identity reasoning as Reactor, see tags.h) -- not a separate tag lookup.
    auto sentinels = engine.Registry().view<SentinelAI>();
    if (sentinels.begin() != sentinels.end()) {
        entt::entity sentinelActor = *sentinels.begin();
        g_logic->AttachView(std::make_unique<AIView>(engine.Registry(), engine.Sounds()), sentinelActor);
    }

    engine.Run(UpdateDrawFrame);

    g_logic.reset();   // drops the attached FlareReactorView too -- g_view becomes dangling
    g_view = nullptr;

    engine.Shutdown();

    return 0;
}
