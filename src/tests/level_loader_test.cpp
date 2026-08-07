#include "doctest/doctest.h"

#include <unordered_map>

#include <entt/entt.hpp>

#include "app/entity/entity_file_parser_yaml.h"
#include "app/entity/level_loader.h"

namespace {
    struct FakePosition {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };
    struct FakeHealth {
        float max = 0.0f;
    };
    struct FakeEnemyTag {};

    void RegisterFakeComponents(EntityFactory &factory) {
        factory.RegisterComponentLoader("Position", [](entt::registry &r, entt::entity e, const EntityDefNode &n) {
            r.emplace<FakePosition>(e, n.Get("x").AsFloat(), n.Get("y").AsFloat(), n.Get("z").AsFloat());
        });
        factory.RegisterComponentLoader("Health", [](entt::registry &r, entt::entity e, const EntityDefNode &n) {
            r.emplace<FakeHealth>(e, n.Get("max").AsFloat());
        });
        factory.RegisterComponentLoader("EnemyTag", [](entt::registry &r, entt::entity e, const EntityDefNode &) {
            r.emplace<FakeEnemyTag>(e);
        });
    }

    // Block style only -- this mini-yaml checkout doesn't parse flow-style maps (see
    // entity_file_parser_yaml.cpp's header comment), unlike ADR-0009's own flow-style example.
    const std::string kEnemyEntityYaml =
        "components:\n"
        "  Position:\n"
        "    x: 0\n"
        "    y: 0\n"
        "    z: 0\n"
        "  Health:\n"
        "    max: 50\n"
        "  EnemyTag:\n";

    const std::string kLevelYaml =
        "actors:\n"
        "  - resource: entities/enemy.yaml\n"
        "    position:\n"
        "      x: 10\n"
        "      y: 0\n"
        "      z: 5\n"
        "  - resource: entities/enemy.yaml\n"
        "    position:\n"
        "      x: 15\n"
        "      y: 0\n"
        "      z: 5\n"
        "    overrides:\n"
        "      Health:\n"
        "        max: 25\n";

    LevelLoader::FileReader FakeFiles() {
        auto files = std::make_shared<std::unordered_map<std::string, std::string>>(
            std::unordered_map<std::string, std::string>{
                {"level.yaml", kLevelYaml},
                {"entities/enemy.yaml", kEnemyEntityYaml},
            });
        return [files](const std::string &path) { return files->at(path); };
    }
}

TEST_CASE("Load spawns one entity per actor placement") {
    entt::registry registry;
    EventManager events;
    EntityFactory factory;
    RegisterFakeComponents(factory);
    YamlEntityFileParser parser;
    LevelLoader loader(factory, parser, FakeFiles());

    std::vector<entt::entity> spawned = loader.Load(registry, events, "level.yaml");

    REQUIRE(spawned.size() == 2);
    CHECK(registry.all_of<FakePosition, FakeHealth, FakeEnemyTag>(spawned[0]));
    CHECK(registry.all_of<FakePosition, FakeHealth, FakeEnemyTag>(spawned[1]));
}

TEST_CASE("Load applies the placement's position as a Position override") {
    entt::registry registry;
    EventManager events;
    EntityFactory factory;
    RegisterFakeComponents(factory);
    YamlEntityFileParser parser;
    LevelLoader loader(factory, parser, FakeFiles());

    std::vector<entt::entity> spawned = loader.Load(registry, events, "level.yaml");

    const FakePosition &firstPos = registry.get<FakePosition>(spawned[0]);
    CHECK(firstPos.x == doctest::Approx(10.0f));
    CHECK(firstPos.z == doctest::Approx(5.0f));

    const FakePosition &secondPos = registry.get<FakePosition>(spawned[1]);
    CHECK(secondPos.x == doctest::Approx(15.0f));
}

TEST_CASE("Load applies explicit overrides on top of the entity resource's base values") {
    entt::registry registry;
    EventManager events;
    EntityFactory factory;
    RegisterFakeComponents(factory);
    YamlEntityFileParser parser;
    LevelLoader loader(factory, parser, FakeFiles());

    std::vector<entt::entity> spawned = loader.Load(registry, events, "level.yaml");

    // First placement has no override -- keeps the resource file's base Health.
    CHECK(registry.get<FakeHealth>(spawned[0]).max == doctest::Approx(50.0f));
    // Second placement overrides Health.max down to 25.
    CHECK(registry.get<FakeHealth>(spawned[1]).max == doctest::Approx(25.0f));
}

TEST_CASE("Load queues EvtData_EntitySpawned for each spawned entity, not dispatched immediately") {
    entt::registry registry;
    EventManager events;
    EntityFactory factory;
    RegisterFakeComponents(factory);
    YamlEntityFileParser parser;
    LevelLoader loader(factory, parser, FakeFiles());

    std::vector<entt::entity> spawnedViaEvent;
    events.Subscribe<EvtData_EntitySpawned>([&](const EvtData_EntitySpawned &e) {
        spawnedViaEvent.push_back(e.entity);
    });

    std::vector<entt::entity> spawned = loader.Load(registry, events, "level.yaml");
    CHECK(spawnedViaEvent.empty());   // Queue()'d, not Emit()'d -- nothing fires until dispatch.

    events.DispatchQueued();

    REQUIRE(spawnedViaEvent.size() == 2);
    CHECK(spawnedViaEvent[0] == spawned[0]);
    CHECK(spawnedViaEvent[1] == spawned[1]);
}
