// LoadRenderMaterial: ADR-0020's ".mat" asset -- a RenderMaterial (ADR-0019) described in YAML
// instead of built by hand in C++ (game/flare_reactor/lighting.cpp's old BuildRimExtras/
// BuildScrollExtras). Reuses ADR-0008's YamlEntityFileParser/EntityDefNode wholesale -- a .mat file
// is just another EntityDefNode-shaped document, no new parser needed (entity_file_parser_yaml.cpp
// itself has no entity-specific assumptions baked in).
#ifndef MATERIAL_LOADER_H
#define MATERIAL_LOADER_H

#include <functional>
#include <string>
#include <vector>

#include "app/entity/entity_def.h"
#include "app/entity/entity_file_parser.h"
#include "app/resource/resource_cache.h"
#include "app/scene/material.h"

// Same shape as LevelLoader::FileReader (app/entity/level_loader.h) -- injectable so
// ParseMaterialExtras' caller-facing sibling (LoadRenderMaterial, material_loader.cpp) stays
// testable against in-memory fake files, not just real ones. Declared locally rather than reusing
// LevelLoader's own nested typedef -- .mat loading isn't a LevelLoader concern, just the same
// pattern.
using FileReader = std::function<std::string(const std::string &path)>;

// Parses a ".mat" file's "extras:" node into RenderMaterial::extras -- one UniformValue per key. A
// child node with its own "r" key parses as a Color, normalized to a vec3 (matches
// resources/shaders/glsl330/lighting.fs's `uniform vec3 rimColor`/`energyColor`, and the old
// BuildRimExtras/BuildScrollExtras' own r/255.0f convention); anything else parses as a plain float
// scalar (rimPower, rimIntensity, scrollSpeed, energyIntensity -- everything else this project's
// shaders have needed so far). Pure -- only EntityDefNode/Vector3/Color (raylib.h types, not linked
// raylib.a symbols) -- unit-tested directly in tests/material_loader_test.cpp, same "headers OK,
// .a not" line hierarchy_test.cpp already draws.
inline std::vector<UniformValue> ParseMaterialExtras(const EntityDefNode &extrasNode) {
    std::vector<UniformValue> extras;
    for (const auto &[name, value] : extrasNode.AsMap()) {
        if (value.HasKey("r")) {
            Color c{static_cast<unsigned char>(value.Get("r").AsInt()),
                     static_cast<unsigned char>(value.Get("g").AsInt()),
                     static_cast<unsigned char>(value.Get("b").AsInt()), 255};
            extras.push_back({name, Vector3{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f}});
        } else {
            extras.push_back({name, value.AsFloat()});
        }
    }
    return extras;
}

// Parses a ".mat" file (matPath) into a real RenderMaterial: LoadShader's the "shader:" stem
// (`.vs`/`.fs`, same two-file convention every shader in this project already uses -- see
// game/flare_reactor/lighting.cpp's old LoadLightingShaderInstance), applies "extras:" via
// ParseMaterialExtras + ApplyExtras (material.h), and loads "textures: { diffuse, specular, normal }"
// directly onto the matching raylib MATERIAL_MAP_* slot (raylib's own default-shader texture0/1/2
// binding convention -- resources/shaders/glsl330/lighting.fs's texture0/texture1 rely on exactly
// this; see material.h's own header comment on why a texture MUST live in raylibMaterial.maps[],
// not extras). `normal` maps to MATERIAL_MAP_NORMAL for forward-compat -- lighting.fs has no
// normal-mapping code path yet, so setting it today is inert but harmless.
//
// Calls LoadShader (a real linked raylib.a symbol) -- deliberately NOT unit-tested directly, same
// exclusion game/flare_reactor/lighting.cpp/skybox.cpp already have (see src/Makefile's TEST_OBJS
// comment). Declared here, defined in material_loader.cpp.
RenderMaterial LoadRenderMaterial(const std::string &matPath, IEntityFileParser &parser,
                                   const FileReader &readFile, ResourceCache<Texture2D> &textures);

// A .mat asset (matTemplate) describes a SHADER treatment (frame vs. core, say) meant to apply to
// many of a Model's materials at once -- but a Model's own materials[] each carry their own
// glTF-authored diffuse map (base color texture/color, one per real material index), which a
// blank/shared .mat has no way to express (one reactor_frame.mat.yaml covers 11 visually different
// base materials). This builds ONE independent RenderMaterial per raylib material INDEX,
// preserving that index's own MATERIAL_MAP_DIFFUSE (from submeshOwn, i.e. model->materials[i]) while
// taking shader/extras/explicit texture overrides (specular/normal) from matTemplate. Deliberately
// builds a fresh LoadMaterialDefault() allocation rather than copying matTemplate.raylibMaterial by
// value -- ::Material::maps is an owned pointer (RL_MALLOC'd), so a naive struct copy would alias
// the SAME maps[] array across every material index built from one shared template, and writing
// one index's diffuse would silently clobber every other index sharing that template.
//
// Calls LoadMaterialDefault (linked raylib.a symbol) -- not unit-tested directly, same exclusion as
// LoadRenderMaterial above.
RenderMaterial MergeSubmeshMaterial(const ::Material &submeshOwn, const RenderMaterial &matTemplate);

#endif // MATERIAL_LOADER_H
