#include "doctest/doctest.h"

#include "app/entity/entity_def.h"

TEST_CASE("A scalar node returns its value via AsString") {
    EntityDefNode node(std::string("hello"));
    CHECK(node.AsString() == "hello");
}

TEST_CASE("AsString falls back on a non-scalar node") {
    EntityDefNode::Map map;
    map.emplace("key", EntityDefNode(std::string("value")));
    EntityDefNode node(map);

    CHECK(node.AsString("fallback") == "fallback");
}

TEST_CASE("AsFloat/AsInt parse a scalar's string value") {
    CHECK(EntityDefNode(std::string("3.5")).AsFloat() == doctest::Approx(3.5f));
    CHECK(EntityDefNode(std::string("42")).AsInt() == 42);
}

TEST_CASE("AsFloat/AsInt fall back on an unparsable or absent value") {
    CHECK(EntityDefNode(std::string("not a number")).AsFloat(9.0f) == doctest::Approx(9.0f));
    CHECK(EntityDefNode().AsInt(-1) == -1);
}

TEST_CASE("HasKey/Get/TryGet on a map node") {
    EntityDefNode::Map map;
    map.emplace("Health", EntityDefNode(std::string("50")));
    EntityDefNode node(map);

    CHECK(node.HasKey("Health"));
    CHECK_FALSE(node.HasKey("Missing"));
    CHECK(node.Get("Health").AsInt() == 50);
    CHECK(node.TryGet("Health") != nullptr);
    CHECK(node.TryGet("Missing") == nullptr);
}

TEST_CASE("Get throws on a missing key") {
    EntityDefNode::Map emptyMap;
    EntityDefNode node(emptyMap);
    CHECK_THROWS_AS(node.Get("Missing"), std::runtime_error);
}

TEST_CASE("Get throws when the node isn't a map at all") {
    EntityDefNode node(std::string("scalar"));
    CHECK_THROWS_AS(node.Get("anything"), std::runtime_error);
}

TEST_CASE("TryGet returns nullptr, not a throw, when the node isn't a map") {
    EntityDefNode node(std::string("scalar"));
    CHECK(node.TryGet("anything") == nullptr);
}

TEST_CASE("AsList returns list contents, empty for a non-list node") {
    EntityDefNode::List list;
    list.emplace_back(std::string("a"));
    list.emplace_back(std::string("b"));
    EntityDefNode node(list);

    REQUIRE(node.AsList().size() == 2);
    CHECK(node.AsList()[0].AsString() == "a");
    CHECK(node.AsList()[1].AsString() == "b");
    CHECK(EntityDefNode(std::string("scalar")).AsList().empty());
}

TEST_CASE("AsMap returns map contents, empty for a non-map node") {
    CHECK(EntityDefNode(std::string("scalar")).AsMap().empty());

    EntityDefNode::Map map;
    map.emplace("x", EntityDefNode(std::string("1")));
    EntityDefNode node(map);
    CHECK(node.AsMap().size() == 1);
}

TEST_CASE("AsBool parses common truthy/falsy spellings, case-insensitively") {
    CHECK(EntityDefNode(std::string("true")).AsBool());
    CHECK(EntityDefNode(std::string("Yes")).AsBool());
    CHECK(EntityDefNode(std::string("1")).AsBool());
    CHECK_FALSE(EntityDefNode(std::string("false")).AsBool(true));
    CHECK_FALSE(EntityDefNode(std::string("No")).AsBool(true));
    CHECK_FALSE(EntityDefNode(std::string("0")).AsBool(true));
}

TEST_CASE("AsBool falls back on unrecognized or absent values") {
    CHECK(EntityDefNode(std::string("maybe")).AsBool(true) == true);
    CHECK(EntityDefNode().AsBool(true) == true);
}

TEST_CASE("MergeOverrides replaces only the components mentioned in overrides") {
    EntityDefNode::Map baseMap;
    baseMap.emplace("Position", EntityDefNode(std::string("base-pos")));
    baseMap.emplace("Health", EntityDefNode(std::string("base-health")));
    EntityDefNode base(baseMap);

    EntityDefNode::Map overridesMap;
    overridesMap.emplace("Health", EntityDefNode(std::string("override-health")));
    EntityDefNode overrides(overridesMap);

    EntityDefNode merged = MergeOverrides(base, overrides);

    CHECK(merged.Get("Position").AsString() == "base-pos");
    CHECK(merged.Get("Health").AsString() == "override-health");
}

TEST_CASE("MergeOverrides with an empty overrides node returns base unchanged") {
    EntityDefNode::Map baseMap;
    baseMap.emplace("Position", EntityDefNode(std::string("base-pos")));
    EntityDefNode base(baseMap);

    EntityDefNode merged = MergeOverrides(base, EntityDefNode(EntityDefNode::Map()));

    CHECK(merged.Get("Position").AsString() == "base-pos");
}
