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

// Draws every entity with both a WorldTransform (docs/adr/0002 -- already propagated by the time
// any IScreenElement runs, see Engine::Run) and a Renderable. Caller is responsible for its own
// BeginMode3D/EndMode3D -- this only issues Draw* calls, same division of responsibility
// GameplayScene already has today.
inline void DrawRenderables(entt::registry &registry) {
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
                    DrawCubeV(position, renderable.size, renderable.color);
                }
                break;
            case Renderable::Shape::Sphere:
                if (renderable.wireframe) {
                    DrawSphereWires(position, renderable.size.x, 8, 8, renderable.color);
                } else {
                    DrawSphere(position, renderable.size.x, renderable.color);
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
