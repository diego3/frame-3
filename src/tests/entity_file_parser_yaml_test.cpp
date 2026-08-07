#include "doctest/doctest.h"

#include "app/entity/entity_file_parser_yaml.h"

// NOTE: block-style YAML (newline + indentation) only -- this mini-yaml checkout doesn't parse
// flow-style maps/sequences ("{ x: 1 }", "[a, b]"); see entity_file_parser_yaml.cpp's header
// comment for how that was confirmed.

TEST_CASE("Parses a nested map into an EntityDefNode map tree") {
    YamlEntityFileParser parser;
    EntityDefNode root = parser.Parse(
        "components:\n"
        "  Position:\n"
        "    x: 1\n"
        "    y: 2\n"
        "    z: 3\n"
        "  Health:\n"
        "    max: 50\n"
    );

    const EntityDefNode &components = root.Get("components");
    const EntityDefNode &position = components.Get("Position");
    CHECK(position.Get("x").AsFloat() == doctest::Approx(1.0f));
    CHECK(position.Get("y").AsFloat() == doctest::Approx(2.0f));
    CHECK(position.Get("z").AsFloat() == doctest::Approx(3.0f));
    CHECK(components.Get("Health").Get("max").AsInt() == 50);
}

TEST_CASE("Parses a block sequence into an EntityDefNode list") {
    YamlEntityFileParser parser;
    EntityDefNode root = parser.Parse(
        "tags:\n"
        "  - a\n"
        "  - b\n"
        "  - c\n"
    );

    const auto &tags = root.Get("tags").AsList();
    REQUIRE(tags.size() == 3);
    CHECK(tags[0].AsString() == "a");
    CHECK(tags[1].AsString() == "b");
    CHECK(tags[2].AsString() == "c");
}

TEST_CASE("A key with no value parses as a scalar, not a map -- fine for a marker component") {
    YamlEntityFileParser parser;
    EntityDefNode root = parser.Parse("components:\n  EnemyTag:\n  Health:\n    max: 1\n");

    const EntityDefNode &tag = root.Get("components").Get("EnemyTag");
    // Confirmed empirically: mini-yaml gives an empty-valued key a single-newline scalar, not a
    // true empty string -- harmless here since a marker component's loader (e.g. EnemyTag's, per
    // ADR-0008) never inspects its data at all, only that the key was present.
    CHECK(tag.AsString() == "\n");
    CHECK(tag.AsMap().empty());
}

TEST_CASE("Invalid YAML throws rather than returning a malformed tree") {
    YamlEntityFileParser parser;
    // A tab character in indentation is invalid YAML (the spec requires spaces) -- mini-yaml
    // rejects it outright, unlike a flow-style construct it just silently misparses (see the
    // .cpp's header comment on the lack of flow-style support).
    CHECK_THROWS(parser.Parse("key:\n\tsub: 1\n"));
}
