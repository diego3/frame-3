#include "app/scene/material_loader.h"

namespace {
    // name -> raylib map slot, for the ".mat" "textures:" node's three supported keys. Named after
    // raylib's own map semantics (diffuse/specular/normal), not the shader's uniform sampler names
    // (texture0/1/2) -- the mapping between the two is raylib's own DrawMesh convention
    // (vendor/raylib/src/rmodels.c), not something a .mat author should need to know.
    int MapSlotForKey(const std::string &key) {
        if (key == "diffuse") return MATERIAL_MAP_DIFFUSE;
        if (key == "specular") return MATERIAL_MAP_SPECULAR;   // == MATERIAL_MAP_METALNESS, same slot (raylib.h)
        if (key == "normal") return MATERIAL_MAP_NORMAL;
        return -1;
    }
}

RenderMaterial LoadRenderMaterial(const std::string &matPath, IEntityFileParser &parser,
                                   const FileReader &readFile, ResourceCache<Texture2D> &textures) {
    EntityDefNode node = parser.Parse(readFile(matPath));

    std::string shaderStem = node.Get("shader").AsString();
    Shader shader = LoadShader((shaderStem + ".vs").c_str(), (shaderStem + ".fs").c_str());

    RenderMaterial material{shader, LoadMaterialDefault(), {}, {}};
    material.raylibMaterial.shader = shader;

    if (const EntityDefNode *extras = node.TryGet("extras")) {
        material.extras = ParseMaterialExtras(*extras);
        ApplyExtras(shader, material.extras);
    }

    if (const EntityDefNode *textureNodes = node.TryGet("textures")) {
        for (const auto &[key, valueNode] : textureNodes->AsMap()) {
            int slot = MapSlotForKey(key);
            if (slot < 0) continue;   // unknown key -- forward-compatible skip, same as EntityFactory's own unknown-component handling

            std::shared_ptr<Texture2D> handle = textures.GetHandle(valueNode.AsString());
            if (!handle) continue;

            material.raylibMaterial.maps[slot].texture = *handle;
            material.textureHandles.push_back(handle);   // keeps the cache handle alive (see material.h)
        }
    }

    return material;
}

RenderMaterial MergeSubmeshMaterial(const ::Material &submeshOwn, const RenderMaterial &matTemplate) {
    RenderMaterial merged{matTemplate.shader, LoadMaterialDefault(), matTemplate.extras, matTemplate.textureHandles};
    merged.raylibMaterial.shader = matTemplate.shader;
    merged.raylibMaterial.maps[MATERIAL_MAP_DIFFUSE] = submeshOwn.maps[MATERIAL_MAP_DIFFUSE];
    merged.raylibMaterial.maps[MATERIAL_MAP_SPECULAR] = matTemplate.raylibMaterial.maps[MATERIAL_MAP_SPECULAR];
    merged.raylibMaterial.maps[MATERIAL_MAP_NORMAL] = matTemplate.raylibMaterial.maps[MATERIAL_MAP_NORMAL];
    return merged;
}
