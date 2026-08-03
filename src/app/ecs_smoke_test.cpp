// Proves EnTT actually compiles, links, and runs as part of this project's build — nothing here
// is real gameplay code. Creates a registry, a couple of plain-data components, a few entities,
// and iterates a view over them once, logging the result through raylib's own TraceLog.
//
// Delete this file (and the call to EcsSmokeTest() in raylib_game.c) once real components and
// systems exist; at that point this smoke test has done its job.
#include "ecs_smoke_test.h"

#include <entt/entt.hpp>
#include <raylib.h>

namespace {

struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

} // namespace

void EcsSmokeTest(void) {
    entt::registry registry;

    for (int i = 0; i < 3; ++i) {
        const auto entity = registry.create();
        registry.emplace<Position>(entity, static_cast<float>(i), 0.0f, 0.0f);
        if (i != 1) {
            // One entity deliberately has no Velocity, to prove the view below only visits
            // entities that have both components.
            registry.emplace<Velocity>(entity, 1.0f, 0.0f, 0.0f);
        }
    }

    int moved = 0;
    auto view = registry.view<Position, Velocity>();
    for (auto [entity, position, velocity] : view.each()) {
        position.x += velocity.x;
        ++moved;
    }

    TraceLog(LOG_INFO, "ECS: EnTT %d.%d.%d smoke test OK — %d entities, %d with Velocity",
             ENTT_VERSION_MAJOR, ENTT_VERSION_MINOR, ENTT_VERSION_PATCH,
             static_cast<int>(registry.storage<Position>().size()), moved);
}
