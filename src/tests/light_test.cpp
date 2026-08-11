#include "doctest/doctest.h"

#include "app/entity/entity_def.h"
#include "app/scene/light.h"

// ParseLightComponent only (docs/adr/0020) -- PushFrameUniforms calls real linked raylib.a symbols
// (SetShaderValue/GetShaderLocation/...), so it stays untested here, same exclusion
// game/flare_reactor/lighting.cpp/skybox.cpp already have (see src/Makefile's TEST_OBJS comment).

TEST_CASE("ParseLightComponent defaults to a white point light when type/color are absent") {
    EntityDefNode node((EntityDefNode::Map()));

    Light light = ParseLightComponent(node);

    CHECK(light.type == Light::Type::Point);
    CHECK(light.color.r == 255);
    CHECK(light.color.g == 255);
    CHECK(light.color.b == 255);
}

TEST_CASE("ParseLightComponent parses type: directional and an r/g/b color") {
    EntityDefNode::Map colorMap;
    colorMap.emplace("r", EntityDefNode(std::string("255")));
    colorMap.emplace("g", EntityDefNode(std::string("196")));
    colorMap.emplace("b", EntityDefNode(std::string("130")));

    EntityDefNode::Map nodeMap;
    nodeMap.emplace("type", EntityDefNode(std::string("directional")));
    nodeMap.emplace("color", EntityDefNode(colorMap));
    EntityDefNode node(nodeMap);

    Light light = ParseLightComponent(node);

    CHECK(light.type == Light::Type::Directional);
    CHECK(light.color.r == 255);
    CHECK(light.color.g == 196);
    CHECK(light.color.b == 130);
    CHECK(light.color.a == 255);
}

TEST_CASE("ParseLightComponent treats any non-'directional' type as Point") {
    EntityDefNode::Map nodeMap;
    nodeMap.emplace("type", EntityDefNode(std::string("point")));
    EntityDefNode node(nodeMap);

    CHECK(ParseLightComponent(node).type == Light::Type::Point);
}
