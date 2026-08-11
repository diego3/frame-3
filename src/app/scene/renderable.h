// Renderable: this project's first render component (docs/rfc/0001-flare-reactor-pipeline-
// experiment.md, step 6). Before this, the only 3D drawing in the product
// (game/sandbox/human_view.cpp's GameplayScene) hardcoded a 1x1x1 MAROON DrawCubeWires per
// WorldTransform -- fine for a single undifferentiated entity, not enough once a scene needs more
// than one visually distinct thing (a gray reactor that turns red, a patrolling sphere). Header-
// only, same shape as transform.h/hierarchy.h -- plain data plus one free function, no class to
// instantiate.
//
// NOTE: an equivalent concern (per-game duplication of "how do I draw the thing this entity
// carries") was independently raised and merged from the claude/camera-fps-second-game-module
// branch -- app/scene/render_components.h's BoxRenderable/app/scene/scene_renderer.h. Deliberately
// left coexisting rather than converged on merge: unifying the two is real new design (an
// indexing/discovery layer across render-component types, not a rename) and belongs in its own
// ADR, not as a side effect of reconciling two branches' file layouts. See
// app/scene/render_components.h's own note and the proposed follow-up ADR revisiting scene-graph
// indexing for where that question actually lives.
#ifndef RENDERABLE_H
#define RENDERABLE_H

#include <memory>
#include <string>

#include <entt/entt.hpp>
#include <raylib.h>
#include <raymath.h>

#include "app/entity/entity_def.h"
#include "app/resource/resource_cache.h"
#include "app/scene/material.h"
#include "app/scene/renderer.h"
#include "app/scene/transform.h"

struct Renderable {
    enum class Shape { Box, Sphere, Model };

    Shape shape = Shape::Box;
    // Sphere reads size.x as its radius (y/z unused). Model reads size as a per-axis scale
    // multiplier applied to the mesh's own authored dimensions (glTF/OBJ/etc. units rarely match
    // this project's world scale -- e.g. game/flare_reactor's reactor_nuclear model, ~35x60x34
    // units raw, needed roughly a 0.03x uniform factor to sit inside the same few-unit footprint
    // the box it replaced did), so this is deliberately tunable per entity in YAML, not derived
    // from the mesh's own bounding box.
    Vector3 size{1.0f, 1.0f, 1.0f};
    Color color = GRAY;   // Model: multiplied over the mesh's own material colors (DrawModelEx's
                           // tint) -- WHITE leaves textures unmodified; BeaconPulseProcess's
                           // gray-to-RED lerp works unchanged on a Model the same way it already
                           // does on a Box/Sphere.
    bool wireframe = true;   // Unused for Model -- DrawModelEx has no wireframe mode of its own.

    // Only set (via a "Renderable" component loader's engine.Models().GetHandle(path) call) when
    // shape == Model; null otherwise. A shared_ptr, not a bare Model, per ResourceCache<T>'s own
    // handle-lifetime contract (engine.h) -- MUST be released before Engine::Shutdown() closes the
    // GL context, same as any other resource-cache handle. Since this one lives inside a component
    // stored in Engine's own entt::registry rather than a caller's local variable, Engine::Shutdown()
    // itself now clears the registry first (engine.cpp) specifically so a live Renderable::model
    // here can't outlive the context its eventual UnloadModel call needs.
    std::shared_ptr<Model> model;
};

// name -> Color for Renderable's "color:" YAML field -- shared by every "Renderable" component
// loader (game/flare_reactor/main.cpp, game/sandbox/screen_gameplay.cpp used to each keep a
// near-identical copy of this; unified here so RegisterComponentLoaders in either file doesn't
// carry it inline). Returns `fallback` for any unrecognized name.
inline Color ParseColorName(const std::string &name, Color fallback) {
    if (name == "red") return RED;
    if (name == "blue") return BLUE;
    if (name == "darkgray" || name == "dark_gray") return DARKGRAY;
    if (name == "green") return GREEN;
    if (name == "orange") return ORANGE;
    if (name == "maroon") return MAROON;
    if (name == "gray" || name == "grey") return GRAY;
    if (name == "white") return WHITE;
    return fallback;
}

// Parses shape/size/color/wireframe/model from a "Renderable" component's own EntityDefNode -- the
// "Renderable" component loader's own body (previously duplicated near-verbatim between
// game/flare_reactor/main.cpp and game/sandbox/screen_gameplay.cpp), factored out so
// RegisterComponentLoaders in either file stays a short list of registrations. `defaultColor` lets
// each game module keep its own "no color given" fallback without forking this function (sandbox
// used MAROON, flare_reactor GRAY -- both match Renderable::color's own struct default when
// defaulted). `models` is only consulted when shape == "model" -- sandbox has never authored one,
// but gains the branch for free; harmless (same "forward-compatible, not a real need yet" shape
// DrawRenderables' own Model case already documents).
inline Renderable ParseRenderableComponent(const EntityDefNode &node, ResourceCache<Model> &models,
                                            Color defaultColor = GRAY) {
    Renderable renderable;
    std::string shapeName = "box";
    if (const EntityDefNode *shape = node.TryGet("shape")) shapeName = shape->AsString("box");
    if (shapeName == "sphere") {
        renderable.shape = Renderable::Shape::Sphere;
    } else if (shapeName == "model") {
        renderable.shape = Renderable::Shape::Model;
    } else {
        renderable.shape = Renderable::Shape::Box;
    }

    if (const EntityDefNode *size = node.TryGet("size")) {
        renderable.size = Vector3{size->Get("x").AsFloat(1.0f), size->Get("y").AsFloat(1.0f),
                                   size->Get("z").AsFloat(1.0f)};
    }
    if (const EntityDefNode *color = node.TryGet("color")) {
        renderable.color = ParseColorName(color->AsString(""), defaultColor);
    }
    if (const EntityDefNode *wireframe = node.TryGet("wireframe")) {
        renderable.wireframe = wireframe->AsBool(true);
    }
    if (renderable.shape == Renderable::Shape::Model) {
        if (const EntityDefNode *model = node.TryGet("model")) {
            renderable.model = models.GetHandle(model->AsString(""));
        }
    }
    return renderable;
}

// Shared, process-lifetime unit geometry (+ one template RenderMaterial) used to draw every solid
// (wireframe == false, non-Model) Renderable -- added alongside game/flare_reactor's lighting work
// so Box/Sphere shapes have real per-vertex normals a custom lighting shader can read. raylib's
// immediate-mode DrawCubeV/DrawSphere (used here before) have none: confirmed against
// vendor/raylib/src/rshapes.c, which never calls rlNormal3f for either -- a custom shader's
// `vertexNormal` attribute would read whatever rlgl's disabled-attribute default is (effectively
// (0,0,0), see rlgl.h), producing degenerate/NaN lighting. GenMeshCube/GenMeshSphere below DO
// compute real normals, so DrawMesh-ing them fixes this for solid Box/Sphere shapes without
// touching wireframe or Model ones.
//
// Lazily built on first call via an immediately-invoked lambda, not a plain namespace-scope global
// -- GenMeshCube/GenMeshSphere/LoadMaterialDefault upload to the GPU, so this can't run before the
// GL context exists (a plain global would construct at static-init time, before InitWindow). Never
// explicitly unloaded -- this is shared engine-primitive geometry, not a per-entity resource-cache
// handle (ADR-0004's handle-lifetime discipline is about handles some component/caller owns and
// must release before Shutdown(); nothing here is owned by any one entity), so it's reclaimed by
// CloseWindow() at process exit the same way raylib's own default shader/texture are.
namespace renderable_detail {
    struct PrimitiveGeometry {
        Mesh cube;
        Mesh sphere;
        // Template only -- DrawRenderables below copies this per draw call and overrides
        // .raylibMaterial's diffuse color from the entity's own Renderable::color (never mutated
        // in place here, unlike the pre-ADR-0019 version of this file). .extras stays empty: the
        // *caller*-supplied RenderMaterial (DrawRenderables' `material` parameter -- e.g. game/
        // flare_reactor's Lighting::GetPrimitivesMaterial()) is what actually carries a shader/
        // extras override, this is only the fallback when no caller material is passed.
        RenderMaterial defaultMaterial;
    };

    inline PrimitiveGeometry &GetPrimitiveGeometry() {
        static PrimitiveGeometry geometry = [] {
            PrimitiveGeometry g{};
            g.cube = GenMeshCube(1.0f, 1.0f, 1.0f);
            g.sphere = GenMeshSphere(1.0f, 16, 16);
            ::Material rl = LoadMaterialDefault();
            g.defaultMaterial = RenderMaterial{rl.shader, rl, {}};
            return g;
        }();
        return geometry;
    }
}

// Draws every entity with both a WorldTransform (docs/adr/0002 -- already propagated by the time
// any IScreenElement runs, see Engine::Run) and a Renderable. Caller is responsible for its own
// BeginMode3D/EndMode3D -- this only issues Draw* calls, same division of responsibility
// GameplayScene already has today.
//
// `material` (ADR-0019), if non-null, supplies the shader + extras bound to every solid Box/Sphere
// Renderable drawn this call (see renderable_detail::PrimitiveGeometry above for why that now has
// real normals to give it) -- e.g. game/flare_reactor/Lighting::GetPrimitivesMaterial(). Wireframe
// Renderables are unaffected (outlines don't benefit from per-pixel lighting; drawn via the old
// unlit immediate-mode calls, unchanged).
//
// Model-shaped Renderables ignore this parameter entirely -- each submesh just uses whichever
// shader/maps raylib's own LoadModel already assigned it (no per-submesh material override path
// here; that's what app/scene/mesh_renderer.h's MeshRenderer is for, ADR-0020 -- this Shape::Model
// case stays as a simpler fallback for a future consumer that doesn't need one). Reimplements
// raylib's own DrawModelEx loop (vendor/raylib/src/rmodels.c) one submesh at a time through
// DrawWithMaterial (app/scene/renderer.h) instead of DrawMesh directly -- same math (scale ->
// rotate(0) -> translate, tint multiplied onto each submesh's own diffuse color via ColorTint),
// just routed through the same Renderer step Box/Sphere use, resolving ADR-0019's "does a Model
// share the Renderer with primitives" open question (yes).
inline void DrawRenderables(entt::registry &registry, const RenderMaterial *material = nullptr) {
    auto view = registry.view<WorldTransform, Renderable>();
    for (auto entity : view) {
        const WorldTransform &world = view.get<WorldTransform>(entity);
        const Renderable &renderable = view.get<Renderable>(entity);
        Vector3 position = Vector3Transform(Vector3Zero(), world.matrix);

        switch (renderable.shape) {
            case Renderable::Shape::Box:
                if (renderable.wireframe) {
                    DrawCubeWiresV(position, renderable.size, renderable.color);
                } else {
                    renderable_detail::PrimitiveGeometry &geometry = renderable_detail::GetPrimitiveGeometry();
                    RenderMaterial draw = material ? *material : geometry.defaultMaterial;
                    draw.raylibMaterial.maps[MATERIAL_MAP_DIFFUSE].color = renderable.color;
                    Matrix transform = MatrixMultiply(
                        MatrixScale(renderable.size.x, renderable.size.y, renderable.size.z),
                        MatrixTranslate(position.x, position.y, position.z));
                    DrawWithMaterial(geometry.cube, draw, transform);
                }
                break;
            case Renderable::Shape::Sphere:
                if (renderable.wireframe) {
                    DrawSphereWires(position, renderable.size.x, 8, 8, renderable.color);
                } else {
                    renderable_detail::PrimitiveGeometry &geometry = renderable_detail::GetPrimitiveGeometry();
                    RenderMaterial draw = material ? *material : geometry.defaultMaterial;
                    draw.raylibMaterial.maps[MATERIAL_MAP_DIFFUSE].color = renderable.color;
                    // renderable.size.x as the radius, matching the old DrawSphere(position,
                    // renderable.size.x, ...) call this replaces -- uniform scale of a unit sphere.
                    Matrix transform = MatrixMultiply(
                        MatrixScale(renderable.size.x, renderable.size.x, renderable.size.x),
                        MatrixTranslate(position.x, position.y, position.z));
                    DrawWithMaterial(geometry.sphere, draw, transform);
                }
                break;
            case Renderable::Shape::Model:
                // rotationAngle 0 -- same fidelity gap DrawCubeV/DrawSphere above already have
                // (position/size/color from WorldTransform + Renderable, rotation not applied;
                // BeaconPulseProcess's LocalTransform::rotation spin has never been visible through
                // this function for any shape). Revisit together if that ever needs fixing.
                if (renderable.model) {
                    Model &model = *renderable.model;
                    Matrix matTransform = MatrixMultiply(
                        MatrixScale(renderable.size.x, renderable.size.y, renderable.size.z),
                        MatrixTranslate(position.x, position.y, position.z));
                    Matrix combined = MatrixMultiply(model.transform, matTransform);

                    for (int i = 0; i < model.meshCount; ++i) {
                        ::Material submaterial = model.materials[model.meshMaterial[i]];
                        submaterial.maps[MATERIAL_MAP_DIFFUSE].color =
                            ColorTint(submaterial.maps[MATERIAL_MAP_DIFFUSE].color, renderable.color);
                        // extras left empty on purpose -- see this function's header comment.
                        RenderMaterial submeshMaterial{submaterial.shader, submaterial, {}};
                        DrawWithMaterial(model.meshes[i], submeshMaterial, combined);
                    }
                }
                break;
        }
    }
}

#endif // RENDERABLE_H
