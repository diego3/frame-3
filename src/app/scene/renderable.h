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

#include <entt/entt.hpp>
#include <raylib.h>
#include <raymath.h>

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

// Shared, process-lifetime unit geometry (+ one mutable Material) used to draw every solid
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
        Material material;      // mutated (.maps[MATERIAL_MAP_DIFFUSE].color/.shader) per draw call
        Shader defaultShader;   // material's own shader before any lighting override, captured once
    };

    inline PrimitiveGeometry &GetPrimitiveGeometry() {
        static PrimitiveGeometry geometry = [] {
            PrimitiveGeometry g{};
            g.cube = GenMeshCube(1.0f, 1.0f, 1.0f);
            g.sphere = GenMeshSphere(1.0f, 16, 16);
            g.material = LoadMaterialDefault();
            g.defaultShader = g.material.shader;
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
// `shader`, if non-null, is bound to every solid Box/Sphere Renderable's shared Material for this
// call (see renderable_detail::PrimitiveGeometry above for why that now has real normals to give
// it). Wireframe Renderables are unaffected (outlines don't benefit from per-pixel lighting; drawn
// via the old unlit immediate-mode calls, unchanged) and so are Model-shaped ones -- a Model's own
// per-material shader (set separately, e.g. by game/flare_reactor's Lighting::ApplyToModel) always
// wins, same as DrawModelEx always did.
inline void DrawRenderables(entt::registry &registry, const Shader *shader = nullptr) {
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
                    geometry.material.maps[MATERIAL_MAP_DIFFUSE].color = renderable.color;
                    geometry.material.shader = shader ? *shader : geometry.defaultShader;
                    Matrix transform = MatrixMultiply(
                        MatrixScale(renderable.size.x, renderable.size.y, renderable.size.z),
                        MatrixTranslate(position.x, position.y, position.z));
                    DrawMesh(geometry.cube, geometry.material, transform);
                }
                break;
            case Renderable::Shape::Sphere:
                if (renderable.wireframe) {
                    DrawSphereWires(position, renderable.size.x, 8, 8, renderable.color);
                } else {
                    renderable_detail::PrimitiveGeometry &geometry = renderable_detail::GetPrimitiveGeometry();
                    geometry.material.maps[MATERIAL_MAP_DIFFUSE].color = renderable.color;
                    geometry.material.shader = shader ? *shader : geometry.defaultShader;
                    // renderable.size.x as the radius, matching the old DrawSphere(position,
                    // renderable.size.x, ...) call this replaces -- uniform scale of a unit sphere.
                    Matrix transform = MatrixMultiply(
                        MatrixScale(renderable.size.x, renderable.size.x, renderable.size.x),
                        MatrixTranslate(position.x, position.y, position.z));
                    DrawMesh(geometry.sphere, geometry.material, transform);
                }
                break;
            case Renderable::Shape::Model:
                // rotationAngle 0 -- same fidelity gap DrawCubeV/DrawSphere above already have
                // (position/size/color from WorldTransform + Renderable, rotation not applied;
                // BeaconPulseProcess's LocalTransform::rotation spin has never been visible through
                // this function for any shape). Revisit together if that ever needs fixing.
                if (renderable.model) {
                    DrawModelEx(*renderable.model, position, Vector3{0.0f, 1.0f, 0.0f}, 0.0f,
                                renderable.size, renderable.color);
                }
                break;
        }
    }
}

#endif // RENDERABLE_H
