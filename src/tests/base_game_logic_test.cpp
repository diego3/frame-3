#include "doctest/doctest.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

#include "app/view/base_game_logic.h"
#include "app/entity/entity_file_parser_yaml.h"

// BaseGameLogic itself touches no raylib -- only entt::registry, EventManager, ProcessManager,
// LevelLoader, all of which are raylib-free too. Tested here with a FakeView standing in for
// HumanView (which does depend on raylib and isn't in the test build's link -- see the Makefile
// comment above TEST_OBJS), the same "prove the pure-logic seam against a stand-in first" pattern
// entity_factory_test.cpp/level_loader_test.cpp already use for EntityFactory/LevelLoader.
namespace {
    class FakeView : public IGameView {
    public:
        void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override {
            id_ = id;
            attachedActorId = actorId;
        }
        void VOnUpdate(float dt) override {
            updateCount++;
            lastDt = dt;
        }
        void VOnRender(float) override { renderCalled = true; }
        GameViewType VGetType() const override { return GameViewType::Other; }

        std::optional<entt::entity> attachedActorId;
        int updateCount = 0;
        float lastDt = 0.0f;
        bool renderCalled = false;
    };

    struct FakePosition {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };

    void RegisterFakeComponents(EntityFactory &factory) {
        factory.RegisterComponentLoader("Position", [](entt::registry &r, entt::entity e, const EntityDefNode &n) {
            r.emplace<FakePosition>(e, n.Get("x").AsFloat(), n.Get("y").AsFloat(), n.Get("z").AsFloat());
        });
    }

    const std::string kEntityYaml =
        "components:\n"
        "  Position:\n"
        "    x: 1\n"
        "    y: 2\n"
        "    z: 3\n";

    const std::string kLevelYaml =
        "actors:\n"
        "  - resource: entity.yaml\n";

    const std::string kTwoActorLevelYaml =
        "actors:\n"
        "  - resource: entity.yaml\n"
        "    position:\n"
        "      x: 10\n"
        "      y: 0\n"
        "      z: 0\n"
        "  - resource: entity.yaml\n"
        "    position:\n"
        "      x: 20\n"
        "      y: 0\n"
        "      z: 0\n";

    LevelLoader::FileReader FakeFiles() {
        auto files = std::make_shared<std::unordered_map<std::string, std::string>>(
            std::unordered_map<std::string, std::string>{
                {"level.yaml", kLevelYaml},
                {"two-actor-level.yaml", kTwoActorLevelYaml},
                {"entity.yaml", kEntityYaml},
            });
        return [files](const std::string &path) { return files->at(path); };
    }

    // Bundles everything BaseGameLogic needs so each TEST_CASE doesn't repeat the same five-object
    // setup -- mirrors how level_loader_test.cpp keeps its own setup inline per case, just factored
    // out one level further since BaseGameLogic sits on top of LevelLoader, not beside it.
    struct Fixture {
        entt::registry registry;
        EventManager events;
        ProcessManager processes;
        EntityFactory factory{};
        YamlEntityFileParser parser;
        LevelLoader levelLoader{factory, parser, FakeFiles()};
        BaseGameLogic logic{registry, events, processes, levelLoader};

        Fixture() { RegisterFakeComponents(factory); }
    };
}

TEST_CASE("BaseGameLogic starts in Loading state") {
    Fixture f;
    CHECK(f.logic.State() == GameLogicState::Loading);
}

TEST_CASE("VLoadLevel transitions Loading to Running and spawns the level's entities") {
    Fixture f;
    f.logic.VLoadLevel("level.yaml");

    CHECK(f.logic.State() == GameLogicState::Running);

    auto view = f.registry.view<FakePosition>();
    REQUIRE(view.size() == 1);
    CHECK(view.get<FakePosition>(*view.begin()).x == doctest::Approx(1.0f));
}

TEST_CASE("VLoadLevel returns spawned entities in the level file's own actors[] order") {
    Fixture f;
    std::vector<entt::entity> spawned = f.logic.VLoadLevel("two-actor-level.yaml");

    REQUIRE(spawned.size() == 2);
    CHECK(f.registry.get<FakePosition>(spawned[0]).x == doctest::Approx(10.0f));
    CHECK(f.registry.get<FakePosition>(spawned[1]).x == doctest::Approx(20.0f));
}

TEST_CASE("AttachView calls VOnAttach with an id and the given actorId, then id is reachable via GetId") {
    Fixture f;
    entt::entity actor = f.registry.create();

    auto fakeView = std::make_unique<FakeView>();
    FakeView *fakeViewPtr = fakeView.get();

    GameViewId id = f.logic.AttachView(std::move(fakeView), actor);

    CHECK(id != 0);
    CHECK(fakeViewPtr->GetId() == id);
    REQUIRE(fakeViewPtr->attachedActorId.has_value());
    CHECK(*fakeViewPtr->attachedActorId == actor);
}

TEST_CASE("AttachView with no actorId leaves the view unpossessed") {
    Fixture f;
    auto fakeView = std::make_unique<FakeView>();
    FakeView *fakeViewPtr = fakeView.get();

    f.logic.AttachView(std::move(fakeView));

    CHECK_FALSE(fakeViewPtr->attachedActorId.has_value());
}

TEST_CASE("Attaching two views gives each a distinct id") {
    Fixture f;
    GameViewId firstId = f.logic.AttachView(std::make_unique<FakeView>());
    GameViewId secondId = f.logic.AttachView(std::make_unique<FakeView>());

    CHECK(firstId != secondId);
}

TEST_CASE("VOnUpdate ticks every attached view's VOnUpdate with the given dt") {
    Fixture f;
    auto fakeView = std::make_unique<FakeView>();
    FakeView *fakeViewPtr = fakeView.get();
    f.logic.AttachView(std::move(fakeView));

    f.logic.VOnUpdate(0.5f);

    CHECK(fakeViewPtr->updateCount == 1);
    CHECK(fakeViewPtr->lastDt == doctest::Approx(0.5f));
}

TEST_CASE("VOnUpdate never calls VOnRender -- rendering isn't reachable through BaseGameLogic") {
    Fixture f;
    auto fakeView = std::make_unique<FakeView>();
    FakeView *fakeViewPtr = fakeView.get();
    f.logic.AttachView(std::move(fakeView));

    f.logic.VOnUpdate(0.016f);

    CHECK_FALSE(fakeViewPtr->renderCalled);
}

TEST_CASE("DetachView removes the view -- it no longer receives VOnUpdate") {
    Fixture f;
    auto fakeView = std::make_unique<FakeView>();
    FakeView *fakeViewPtr = fakeView.get();
    GameViewId id = f.logic.AttachView(std::move(fakeView));

    f.logic.DetachView(id);
    f.logic.VOnUpdate(0.016f);

    CHECK(fakeViewPtr->updateCount == 0);
}

TEST_CASE("Pause() stops VOnUpdate from reaching attached views; Resume() restores it") {
    Fixture f;
    auto fakeView = std::make_unique<FakeView>();
    FakeView *fakeViewPtr = fakeView.get();
    f.logic.AttachView(std::move(fakeView));

    f.logic.Pause();
    f.logic.VOnUpdate(0.016f);
    CHECK(fakeViewPtr->updateCount == 0);
    CHECK(f.logic.State() == GameLogicState::Paused);

    f.logic.Resume();
    f.logic.VOnUpdate(0.016f);
    CHECK(fakeViewPtr->updateCount == 1);
}
