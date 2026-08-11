#include "doctest/doctest.h"

#include <raylib.h>

#include "app/scene/mesh_renderer.h"

// HasEmissiveMap only (docs/adr/0020) -- DrawMeshRenderers/UpdateMeshRendererFrameUniforms call
// real linked raylib.a symbols (DrawMesh/SetShaderValue/...), so they stay untested here, same
// exclusion game/flare_reactor/lighting.cpp/skybox.cpp already have (see src/Makefile's TEST_OBJS
// comment). MaterialMap arrays below are hand-built, not LoadMaterialDefault()'d (a linked call) --
// pure struct-field reads only, same "raylib headers OK, raylib.a not" line hierarchy_test.cpp
// already draws.

TEST_CASE("HasEmissiveMap is false for a material with a zeroed MATERIAL_MAP_EMISSION slot") {
    MaterialMap maps[16] = {};
    ::Material material{};
    material.maps = maps;

    CHECK_FALSE(HasEmissiveMap(material));
}

TEST_CASE("HasEmissiveMap is true when MATERIAL_MAP_EMISSION carries a real color") {
    MaterialMap maps[16] = {};
    maps[MATERIAL_MAP_EMISSION].color = Color{255, 255, 255, 255};
    ::Material material{};
    material.maps = maps;

    CHECK(HasEmissiveMap(material));
}

TEST_CASE("HasEmissiveMap is true when MATERIAL_MAP_EMISSION carries a real texture") {
    MaterialMap maps[16] = {};
    maps[MATERIAL_MAP_EMISSION].texture.id = 7;   // any non-zero id -- a real GPU texture handle
    ::Material material{};
    material.maps = maps;

    CHECK(HasEmissiveMap(material));
}

TEST_CASE("HasEmissiveMap ignores other slots, e.g. a normal diffuse material") {
    MaterialMap maps[16] = {};
    maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    maps[MATERIAL_MAP_DIFFUSE].texture.id = 3;
    ::Material material{};
    material.maps = maps;

    CHECK_FALSE(HasEmissiveMap(material));
}
