/**********************************************************************************************
*
*   raylib - Advance Game template
*
*   Gameplay Screen Functions Definitions (Init, Update, Draw, Unload)
*
*   Copyright (c) 2014-2022 Ramon Santamaria (@raysan5)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#include "raylib.h"
#include "screens.h"

#include <memory>

#include "app/base_game_logic.h"
#include "app/engine.h"
#include "app/entity_factory.h"
#include "app/entity_file_parser_yaml.h"
#include "app/level_loader.h"
#include "app/transform.h"
#include "human_view.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int framesCounter = 0;
static int finishScreen = 0;

namespace {
    // File-local state held across separate Init/Update/Draw/Unload calls -- the same pattern
    // main.cpp already uses for screen-transition state (ADR-0010 Consequences flagged this shape
    // without fully specifying it; this is that specification). Used to live behind an extern "C"
    // bridge (gameplay_bridge.cpp) while this file was still plain C; ADR-0014's migration removed
    // that indirection once this screen became a real C++ translation unit -- BaseGameLogic/
    // HumanView are reachable directly.
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

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

// Gameplay Screen Initialization logic
void InitGameplayScreen(void)
{
    framesCounter = 0;
    finishScreen = 0;

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

    auto humanView = std::make_unique<HumanView>(engine->Registry(), engine->Processes(), engine->Sounds());
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

// Gameplay Screen Update logic
void UpdateGameplayScreen(void)
{
    if (g_logic) g_logic->VOnUpdate(GetFrameTime());

    // Press enter or tap to change to ENDING screen
    if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
    {
        finishScreen = 1;
        PlaySound(fxCoin);
    }
}

// Gameplay Screen Draw logic
void DrawGameplayScreen(void)
{
    if (g_humanView) g_humanView->VOnRender(GetFrameTime());

    DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 20, 20, MAROON);
}

// Gameplay Screen Unload logic
void UnloadGameplayScreen(void)
{
    g_logic.reset();          // drops the attached HumanView too -- g_humanView becomes dangling
    g_humanView = nullptr;
    g_levelLoader.reset();
    g_entityFactory.reset();
}

// Gameplay Screen should finish?
int FinishGameplayScreen(void)
{
    return finishScreen;
}
