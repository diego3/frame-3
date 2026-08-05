#include "gameplay_bridge.h"

#include <memory>

#include <raylib.h>

#include "base_game_logic.h"
#include "engine.h"
#include "entity_factory.h"
#include "entity_file_parser_yaml.h"
#include "human_view.h"
#include "level_loader.h"
#include "transform.h"

namespace {
    // File-local state held across separate Init/Update/Draw/Unload calls -- the same pattern
    // raylib_game.cpp already uses for screen-transition state (ADR-0010 Consequences flagged
    // this shape without fully specifying it; this is that specification).
    YamlEntityFileParser g_parser;
    std::unique_ptr<EntityFactory> g_entityFactory;
    std::unique_ptr<LevelLoader> g_levelLoader;
    std::unique_ptr<BaseGameLogic> g_logic;
    HumanView *g_humanView = nullptr;   // non-owning -- g_logic owns it via views_

    // The first real component loader wired into the product (every prior caller of EntityFactory
    // used fakes -- entity_factory_test.cpp, level_loader_test.cpp). "Position" -> LocalTransform
    // + WorldTransform is deliberately the only one: ADR-0010's own Open Questions leave "render
    // component design" undecided, so this stays scoped to just enough to place an entity in
    // space, not a full component schema nobody's asked for yet.
    void RegisterComponentLoaders(EntityFactory &factory) {
        factory.RegisterComponentLoader("Position", [](entt::registry &registry, entt::entity entity,
                                                         const EntityDefNode &node) {
            registry.emplace<LocalTransform>(
                entity, Vector3{node.Get("x").AsFloat(), node.Get("y").AsFloat(), node.Get("z").AsFloat()});
            registry.emplace<WorldTransform>(entity);
        });
    }
}

void GameplayBridge_Init(void) {
    Engine *engine = Engine::Current();

    g_entityFactory = std::make_unique<EntityFactory>([](const std::string &name) {
        TraceLog(LOG_WARNING, "Unknown component '%s' in entity definition, skipping", name.c_str());
    });
    RegisterComponentLoaders(*g_entityFactory);

    g_levelLoader = std::make_unique<LevelLoader>(*g_entityFactory, g_parser);
    g_logic = std::make_unique<BaseGameLogic>(engine->Registry(), engine->Events(), engine->Processes(),
                                               *g_levelLoader);

    // Deviates from ADR-0010 Sec 3's literal ordering ("attaches a HumanView, calls VLoadLevel") --
    // load first, then attach, because HumanView needs a real entity to possess (actorId) and
    // there's no entity to name before the level actually spawns one. BaseGameLogic itself doesn't
    // enforce attach-before-load ordering (nothing in VLoadLevel/AttachView depends on the other
    // having run first), so this is a safe reordering, not a workaround for a real constraint.
    g_logic->VLoadLevel("resources/levels/level_01.yaml");

    auto humanView = std::make_unique<HumanView>(engine->Registry());
    g_humanView = humanView.get();

    // First entity in the registry stands in for "the player" until a real PlayerTag/possession
    // mechanism exists -- level_01.yaml (assets/levels/) currently spawns exactly one entity, so
    // this is unambiguous today; revisit once a level has more than one entity and "which one is
    // the player" needs a real answer.
    std::optional<entt::entity> playerActor;
    auto positioned = engine->Registry().view<LocalTransform>();
    if (positioned.begin() != positioned.end()) playerActor = *positioned.begin();

    g_logic->AttachView(std::move(humanView), playerActor);
}

void GameplayBridge_Update(float dt) {
    if (g_logic) g_logic->VOnUpdate(dt);
}

void GameplayBridge_Draw(void) {
    if (g_humanView) g_humanView->VOnRender(GetFrameTime());
}

void GameplayBridge_Unload(void) {
    g_logic.reset();          // drops the attached HumanView too -- g_humanView becomes dangling
    g_humanView = nullptr;
    g_levelLoader.reset();
    g_entityFactory.reset();
}
