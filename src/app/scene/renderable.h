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

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/scene/transform.h"

struct Renderable {
    enum class Shape { Box, Sphere };

    Shape shape = Shape::Box;
    Vector3 size{1.0f, 1.0f, 1.0f};   // Sphere reads size.x as its radius; y/z unused for Sphere.
    Color color = GRAY;
    bool wireframe = true;
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
        }
    }
}

#endif // RENDERABLE_H
