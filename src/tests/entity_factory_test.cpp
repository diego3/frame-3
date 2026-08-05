#include "doctest/doctest.h"

#include <entt/entt.hpp>

#include "app/entity_def.h"
#include "app/entity_factory.h"

namespace {
    // Fake components (mirrors ADR-0008's own Position/Health/EnemyTag example, and the
    // fake-loader-instead-of-real-type pattern resource_cache_test.cpp already uses) -- nothing
    // real about these, just enough to prove EntityFactory dispatches to the right loader.
    struct FakeHealth {
        float max = 0.0f;
    };
    struct FakeTag {};
}

TEST_CASE("Create emplaces components via their registered loaders") {
    entt::registry registry;
    EntityFactory factory;
    factory.RegisterComponentLoader("Health", [](entt::registry &r, entt::entity e, const EntityDefNode &n) {
        r.emplace<FakeHealth>(e, n.Get("max").AsFloat());
    });
    factory.RegisterComponentLoader("Tag", [](entt::registry &r, entt::entity e, const EntityDefNode &) {
        r.emplace<FakeTag>(e);
    });

    EntityDefNode::Map healthData;
    healthData.emplace("max", EntityDefNode(std::string("75")));
    EntityDefNode::Map def;
    def.emplace("Health", EntityDefNode(healthData));
    def.emplace("Tag", EntityDefNode(EntityDefNode::Map()));

    entt::entity entity = factory.Create(registry, EntityDefNode(def));

    REQUIRE(registry.all_of<FakeHealth>(entity));
    CHECK(registry.get<FakeHealth>(entity).max == doctest::Approx(75.0f));
    CHECK(registry.all_of<FakeTag>(entity));
}

TEST_CASE("Create skips a component with no registered loader, without failing the whole entity") {
    entt::registry registry;
    EntityFactory factory;
    factory.RegisterComponentLoader("Tag", [](entt::registry &r, entt::entity e, const EntityDefNode &) {
        r.emplace<FakeTag>(e);
    });

    EntityDefNode::Map def;
    def.emplace("Tag", EntityDefNode(EntityDefNode::Map()));
    def.emplace("SomeFutureComponent", EntityDefNode(std::string("data")));

    entt::entity entity = factory.Create(registry, EntityDefNode(def));

    CHECK(registry.all_of<FakeTag>(entity));
    CHECK_FALSE(registry.all_of<FakeHealth>(entity));
}

TEST_CASE("Create reports each unknown component name to the handler") {
    entt::registry registry;
    std::vector<std::string> unknownNames;
    EntityFactory factory([&](const std::string &name) { unknownNames.push_back(name); });

    EntityDefNode::Map def;
    def.emplace("Unknown", EntityDefNode(std::string("data")));

    factory.Create(registry, EntityDefNode(def));

    REQUIRE(unknownNames.size() == 1);
    CHECK(unknownNames[0] == "Unknown");
}

TEST_CASE("Create on a non-map definition returns an entity with no components, no crash") {
    entt::registry registry;
    EntityFactory factory;

    entt::entity entity = factory.Create(registry, EntityDefNode(std::string("not a map")));

    CHECK(registry.valid(entity));
}
