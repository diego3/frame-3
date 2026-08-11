#include "doctest/doctest.h"

#include <raylib.h>

#include "app/entity/entity_def.h"
#include "app/scene/material_loader.h"

// ParseMaterialExtras only (docs/adr/0020) -- LoadRenderMaterial/MergeSubmeshMaterial call
// LoadShader/LoadMaterialDefault (real linked raylib.a symbols), so they stay untested here, same
// exclusion game/flare_reactor/lighting.cpp/skybox.cpp already have (see src/Makefile's TEST_OBJS
// comment).

TEST_CASE("ParseMaterialExtras parses a plain scalar as a float") {
    EntityDefNode::Map extrasMap;
    extrasMap.emplace("rimPower", EntityDefNode(std::string("3.0")));
    EntityDefNode extras(extrasMap);

    std::vector<UniformValue> parsed = ParseMaterialExtras(extras);

    REQUIRE(parsed.size() == 1);
    CHECK(parsed[0].name == "rimPower");
    CHECK(std::get<float>(parsed[0].value) == doctest::Approx(3.0f));
}

TEST_CASE("ParseMaterialExtras parses an r/g/b-keyed node as a normalized Color->Vector3") {
    EntityDefNode::Map colorMap;
    colorMap.emplace("r", EntityDefNode(std::string("110")));
    colorMap.emplace("g", EntityDefNode(std::string("210")));
    colorMap.emplace("b", EntityDefNode(std::string("255")));

    EntityDefNode::Map extrasMap;
    extrasMap.emplace("rimColor", EntityDefNode(colorMap));
    EntityDefNode extras(extrasMap);

    std::vector<UniformValue> parsed = ParseMaterialExtras(extras);

    REQUIRE(parsed.size() == 1);
    CHECK(parsed[0].name == "rimColor");
    Vector3 v = std::get<Vector3>(parsed[0].value);
    CHECK(v.x == doctest::Approx(110.0f / 255.0f));
    CHECK(v.y == doctest::Approx(210.0f / 255.0f));
    CHECK(v.z == doctest::Approx(1.0f));
}

TEST_CASE("ParseMaterialExtras handles a mix of scalar and color entries, and an empty node") {
    EntityDefNode::Map colorMap;
    colorMap.emplace("r", EntityDefNode(std::string("0")));
    colorMap.emplace("g", EntityDefNode(std::string("0")));
    colorMap.emplace("b", EntityDefNode(std::string("0")));

    EntityDefNode::Map extrasMap;
    extrasMap.emplace("energyColor", EntityDefNode(colorMap));
    extrasMap.emplace("scrollSpeed", EntityDefNode(std::string("0.15")));
    EntityDefNode extras(extrasMap);

    CHECK(ParseMaterialExtras(extras).size() == 2);
    CHECK(ParseMaterialExtras(EntityDefNode(EntityDefNode::Map())).empty());
}
