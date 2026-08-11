// MeshRenderer: ADR-0020's "MeshRenderer/Material/Shader" component -- the piece Renderable
// (renderable.h) never had for its Shape::Model case: a way for one submesh of a multi-material
// Model to carry its own RenderMaterial, independent of every other submesh. Born from two real
// bugs building game/flare_reactor's scroll effect: (1) Lighting::ApplyToModel applied rim+scroll
// to all 12 of the reactor's materials uniformly, with no way to scope them to just the glowing
// core; (2) MATERIAL_MAP_SPECULAR/MATERIAL_MAP_METALNESS share one raylib slot, so running that
// same call on a second, unrelated model (survival_guitar_backpack, real PBR metalness) would have
// silently corrupted its metalness texture. See docs/adr/0020 for the full design.
//
// A Model with N materials becomes N (well, N submeshes -- see below) sibling child entities under
// one root, via Relationship/LocalTransform/WorldTransform (hierarchy.h, ADR-0002) -- not a new
// hierarchy mechanism, just applying the one that already exists. Each child's MeshRenderer
// references its own RenderMaterial (ADR-0019); the reactor's core submesh and its frame submeshes
// can finally diverge, and the backpack (a fully separate subtree) never shares state with either.
//
// Header-only, same shape as transform.h/hierarchy.h/renderable.h -- plain data plus free
// functions, no class to instantiate.
#ifndef MESH_RENDERER_H
#define MESH_RENDERER_H

#include <memory>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/entity/entity_def.h"
#include "app/resource/resource_cache.h"
#include "app/scene/hierarchy.h"
#include "app/scene/light.h"
#include "app/scene/material.h"
#include "app/scene/material_loader.h"
#include "app/scene/renderer.h"
#include "app/scene/transform.h"

struct MeshRenderer {
    Mesh mesh;                                 // one submesh of some Model -- a non-owning value
                                                // copy (raylib Mesh holds GPU buffer IDs, not RAM
                                                // it owns independently); see MeshRendererRoot below
                                                // for what keeps the owning Model alive.
    std::shared_ptr<RenderMaterial> material;  // this entity's own -- never mutated by, or shared
                                                // state with, any other entity's subtree, unlike
                                                // Lighting::ApplyToModel's old direct Model.materials[]
                                                // mutation. Shared only with sibling submeshes that
                                                // came from the same raylib material INDEX (built once
                                                // per index by the "MeshRenderer" component loader via
                                                // material_loader.h's MergeSubmeshMaterial -- one
                                                // Model material can back many meshes, e.g. the
                                                // reactor's pCylinder63-66 all use material index 3).
    Color tint = WHITE;                        // multiplied over the material's own diffuse color
                                                // each draw -- same role Renderable::color played
                                                // for the old Shape::Model draw path; WHITE leaves
                                                // the material's own baked color/textures unskewed.
                                                // BeaconPulseProcess (game/flare_reactor) lerps this
                                                // toward RED the same way it used to lerp
                                                // Renderable::color.
};

// Lifetime anchor only -- never drawn directly, carries no visual meaning of its own.
struct MeshRendererRoot {
    // A raylib Mesh copied into a MeshRenderer child (above) is a non-owning view into GPU buffers
    // actually owned by the Model those meshes came from; this keeps that Model's
    // ResourceCache<Model> handle (Engine::Models(), ADR-0004) alive for as long as any of its
    // submesh entities might still reference one of its Mesh entries.
    std::shared_ptr<Model> model;

    // Real bug hit building this (2026-08-11, both the reactor and the backpack rendering broken --
    // root-caused via a shader-id TraceLog trail showing the .mat shaders unloaded mid-level-load,
    // before any frame drew): MergeSubmeshMaterial only copies a Shader's raw {id, locs} VALUE into
    // each per-index RenderMaterial it builds (material_loader.h) -- it does NOT extend the
    // ResourceCache<RenderMaterial> handle's ownership. If nothing else holds a shared_ptr to the
    // ORIGINAL cached template (materials.GetHandle("...frame.mat.yaml")/("...core.mat.yaml")) past
    // the "MeshRenderer" component loader returning, that cache entry's last reference drops right
    // there -- its deleter (UnloadShader) fires immediately, and every child's copied Shader.id now
    // refers to an already-destroyed (or, worse, later-reused-by-something-else) GL program. This
    // vector is that missing owner: the exact frameMaterial/coreMaterial handles the loader resolved,
    // kept alive for as long as this root (and therefore its children) exists.
    std::vector<std::shared_ptr<RenderMaterial>> materialTemplates;
};

// True if `material`'s MATERIAL_MAP_EMISSION slot carries real data -- raylib's own glTF loader
// (vendor/raylib/src/rmodels.c's LoadGLTF) populates this slot's color/texture directly from the
// source file's own emissiveFactor/emissiveTexture (glTF spec), so this reads the model's own
// authored intent ("this part should glow") instead of guessing by material name or index. Reactor
// example (docs/adr/0020): the reactor_nuclear glTF's 12 materials are named anisotropic19,
// lambert1, blinn10, phongE9... pure Sketchfab export artifacts with no semantic hint which one is
// "the core" -- but 3 of them (phongE9, anisotropic20, lambert7) DO carry a real emissiveFactor,
// confirmed by reading the glTF's own JSON. A material this project's own code never wrote to
// (LoadMaterialDefault/a freshly-allocated Model materials[] entry) leaves this slot zeroed, so a
// non-emissive material reads false here.
//
// Pure struct-field read, no linked raylib symbols -- unit-testable directly (tests/mesh_renderer_test.cpp)
// against a hand-built ::Material, same "raylib headers OK, raylib.a not" line hierarchy_test.cpp
// already draws.
inline bool HasEmissiveMap(const ::Material &material) {
    const MaterialMap &emission = material.maps[MATERIAL_MAP_EMISSION];
    return emission.texture.id > 0 || emission.color.r > 0 || emission.color.g > 0 || emission.color.b > 0;
}

// Draws every entity with both a WorldTransform and a MeshRenderer -- same BeginMode3D/EndMode3D
// division of responsibility DrawRenderables (renderable.h) already has; caller issues those.
// Routes through the same DrawWithMaterial (renderer.h) DrawRenderables' Model path already used,
// just one call per submesh entity instead of one call per submesh index inside a single Renderable's
// loop.
inline void DrawMeshRenderers(entt::registry &registry) {
    auto view = registry.view<WorldTransform, MeshRenderer>();
    for (auto entity : view) {
        const WorldTransform &world = view.get<WorldTransform>(entity);
        const MeshRenderer &meshRenderer = view.get<MeshRenderer>(entity);
        if (!meshRenderer.material) continue;

        RenderMaterial draw = *meshRenderer.material;
        draw.raylibMaterial.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(draw.raylibMaterial.maps[MATERIAL_MAP_DIFFUSE].color, meshRenderer.tint);
        DrawWithMaterial(meshRenderer.mesh, draw, world.matrix);
    }
}

// Pushes light.h's PushFrameUniforms to every distinct MeshRenderer entity's own shader this frame
// -- call once per frame, before DrawMeshRenderers (same "Update() before Draw()" split
// game/flare_reactor/lighting.cpp's own Lighting::Update/DrawRenderables pair already established;
// frame-scoped uniforms are pushed once here, per-material extras are re-applied per draw call
// inside BindMaterial/DrawWithMaterial itself, same as always). Pushes once per ENTITY, not once
// per distinct Shader -- sibling submeshes sharing one .mat (hence one compiled Shader instance)
// get redundant SetShaderValue calls, same accepted per-frame cost PushFrameUniforms's own header
// comment already documents; revisit together if that ever stops being negligible at this
// project's scale.
inline void UpdateMeshRendererFrameUniforms(entt::registry &registry, Vector3 viewPos) {
    auto view = registry.view<MeshRenderer>();
    for (auto entity : view) {
        const MeshRenderer &meshRenderer = view.get<MeshRenderer>(entity);
        if (meshRenderer.material) PushFrameUniforms(registry, viewPos, meshRenderer.material->shader);
    }
}

// Parses "model:"/"size:"/"material:"/"coreMaterial:" from a "MeshRenderer" component's own
// EntityDefNode and spawns the subtree onto `entity` (see this header's own top comment) -- the
// "MeshRenderer" component loader's own body (game/flare_reactor/main.cpp), factored out so
// RegisterComponentLoaders there stays a short list of registrations. A no-op if `model`/`material`
// don't resolve to a real handle (e.g. a bad path) -- same forward-compatible "skip, don't crash"
// tolerance the rest of this project's component loaders already have.
inline void SpawnMeshRendererComponent(entt::registry &registry, entt::entity entity, const EntityDefNode &node,
                                        ResourceCache<Model> &models, ResourceCache<RenderMaterial> &materials) {
    auto model = models.GetHandle(node.Get("model").AsString(""));
    if (!model) return;

    Vector3 size{1.0f, 1.0f, 1.0f};
    if (const EntityDefNode *sizeNode = node.TryGet("size")) {
        size = Vector3{sizeNode->Get("x").AsFloat(1.0f), sizeNode->Get("y").AsFloat(1.0f),
                       sizeNode->Get("z").AsFloat(1.0f)};
    }

    std::shared_ptr<RenderMaterial> frameMaterial = materials.GetHandle(node.Get("material").AsString(""));
    std::shared_ptr<RenderMaterial> coreMaterial;
    if (const EntityDefNode *core = node.TryGet("coreMaterial")) {
        coreMaterial = materials.GetHandle(core->AsString(""));
    }
    if (!frameMaterial) return;

    // frameMaterial/coreMaterial themselves, not just `model` -- see MeshRendererRoot's own header
    // comment for why: MergeSubmeshMaterial below only copies each template's Shader by value into
    // every per-index RenderMaterial it builds, so something has to keep the ORIGINAL
    // ResourceCache<RenderMaterial> handle alive past this function returning, or its refcount hits
    // zero right here and UnloadShader fires under every child's feet.
    registry.emplace<MeshRendererRoot>(entity, MeshRendererRoot{model, {frameMaterial, coreMaterial}});

    // One merged RenderMaterial per raylib material index (material_loader.h's
    // MergeSubmeshMaterial), reused across every mesh that shares that index -- same granularity
    // the old Lighting::ApplyToModel used to compile a shader instance at. "Core" is decided
    // per-index via HasEmissiveMap above, not by name/index -- see its own header comment.
    std::vector<std::shared_ptr<RenderMaterial>> perIndexMaterial(model->materialCount);
    for (int i = 0; i < model->materialCount; ++i) {
        bool isCore = coreMaterial && HasEmissiveMap(model->materials[i]);
        const RenderMaterial &matTemplate = isCore ? *coreMaterial : *frameMaterial;
        perIndexMaterial[i] = std::make_shared<RenderMaterial>(MergeSubmeshMaterial(model->materials[i], matTemplate));
    }

    for (int i = 0; i < model->meshCount; ++i) {
        entt::entity submesh = registry.create();
        SetParent(registry, submesh, entity);
        // `size` on each CHILD's own LocalTransform.scale, not the root entity's -- deliberately
        // avoids depending on "Position" (which writes the root's LocalTransform) having already
        // run: EntityDefNode::AsMap() is an unordered_map, so component loader order is otherwise
        // unspecified, and this is the first loader that would otherwise need one.
        registry.emplace<LocalTransform>(submesh, LocalTransform{Vector3{0.0f, 0.0f, 0.0f}, QuaternionIdentity(), size});
        registry.emplace<WorldTransform>(submesh);
        registry.emplace<MeshRenderer>(submesh, MeshRenderer{model->meshes[i], perIndexMaterial[model->meshMaterial[i]], WHITE});
    }
}

#endif // MESH_RENDERER_H
