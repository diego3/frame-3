#include "doctest/doctest.h"

#include "app/entity/entity_def.h"
#include "app/scene/renderable.h"

// ParseColorName/ParseRenderableComponent only (previously duplicated near-verbatim between
// game/flare_reactor/main.cpp and game/sandbox/screen_gameplay.cpp, unified here -- see this
// header's own comment). ResourceCache<Model> works with a fake loader/unloader here, same
// no-raylib-linked approach resource_cache_test.cpp already uses for the cache itself -- Model
// (raylib.h) is a plain struct, no linked symbol needed to default-construct one. DrawRenderables
// itself stays untested (real linked raylib.a calls), same exclusion every other draw function in
// this project already has.

TEST_CASE("ParseColorName resolves a known name, falls back on an unknown one") {
    CHECK(ParseColorName("red", BLACK).r == RED.r);
    CHECK(ParseColorName("darkgray", BLACK).r == DARKGRAY.r);
    Color fallback = ParseColorName("not-a-color", MAGENTA);
    CHECK(fallback.r == MAGENTA.r);
    CHECK(fallback.g == MAGENTA.g);
    CHECK(fallback.b == MAGENTA.b);
}

TEST_CASE("ParseRenderableComponent parses shape/size/color/wireframe, defaults to Box") {
    ResourceCache<Model> models([](const char *) { return Model{}; }, [](Model &) {});

    EntityDefNode::Map sizeMap;
    sizeMap.emplace("x", EntityDefNode(std::string("2")));
    sizeMap.emplace("y", EntityDefNode(std::string("3")));
    sizeMap.emplace("z", EntityDefNode(std::string("4")));

    EntityDefNode::Map nodeMap;
    nodeMap.emplace("shape", EntityDefNode(std::string("sphere")));
    nodeMap.emplace("size", EntityDefNode(sizeMap));
    nodeMap.emplace("color", EntityDefNode(std::string("red")));
    nodeMap.emplace("wireframe", EntityDefNode(std::string("false")));
    EntityDefNode node(nodeMap);

    Renderable renderable = ParseRenderableComponent(node, models);

    CHECK(renderable.shape == Renderable::Shape::Sphere);
    CHECK(renderable.size.x == doctest::Approx(2.0f));
    CHECK(renderable.size.y == doctest::Approx(3.0f));
    CHECK(renderable.size.z == doctest::Approx(4.0f));
    CHECK(renderable.color.r == RED.r);
    CHECK_FALSE(renderable.wireframe);
    CHECK(renderable.model == nullptr);   // shape != model -- never touches the cache
}

TEST_CASE("ParseRenderableComponent defaults to Box/wireframe and uses the caller's defaultColor") {
    ResourceCache<Model> models([](const char *) { return Model{}; }, [](Model &) {});
    EntityDefNode node((EntityDefNode::Map()));

    Renderable renderable = ParseRenderableComponent(node, models, MAROON);

    CHECK(renderable.shape == Renderable::Shape::Box);
    CHECK(renderable.wireframe);
    CHECK(renderable.color.r == GRAY.r);   // Renderable's own struct default -- "color" was never given
}

TEST_CASE("ParseRenderableComponent resolves shape: model through the ResourceCache") {
    int loadCount = 0;
    ResourceCache<Model> models(
        [&](const char *) {
            loadCount++;
            return Model{};
        },
        [](Model &) {});

    EntityDefNode::Map nodeMap;
    nodeMap.emplace("shape", EntityDefNode(std::string("model")));
    nodeMap.emplace("model", EntityDefNode(std::string("resources/models/reactor_nuclear/scene.gltf")));
    EntityDefNode node(nodeMap);

    Renderable renderable = ParseRenderableComponent(node, models);

    CHECK(renderable.shape == Renderable::Shape::Model);
    CHECK(renderable.model != nullptr);
    CHECK(loadCount == 1);
}
