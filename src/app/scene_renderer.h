// A minimal, generic "draw what the scene graph says is here" step -- reads every entity with a
// BoxRenderable (render_components.h) at its already-computed WorldTransform (ADR-0002's
// Relationship/LocalTransform hierarchy, propagated by PropagateTransforms), so a view's own scene
// element doesn't have to hand-roll this loop itself. Game-agnostic by construction (it only
// touches app/ types), so any HumanView-family view can call it -- game/camera_fps/human_view.cpp
// is the first (docs/adr/0017 follow-up); game/sandbox's own GameplayScene still hardcodes its
// entity draw calls instead, since sandbox's player entity has no BoxRenderable yet.
//
// Deliberately does NOT open/close BeginMode3D/EndMode3D or touch a Camera3D -- which camera is
// active, and how a view stores one, differs per game (game/sandbox/human_view.cpp keeps a plain
// Camera3D member; game/camera_fps keeps one on a FirstPersonCameraRig component). That's the
// caller's job, same as raylib's own DrawCubeV/DrawPlane/etc. already expect to run inside an
// already-open BeginMode3D block.
#ifndef SCENE_RENDERER_H
#define SCENE_RENDERER_H

#include <entt/entt.hpp>

// raylib.h before raymath.h -- see game/camera_fps/human_view.cpp's own comment on this; raymath.h
// only skips its own conflicting Vector2/Vector3/Matrix struct definitions once RAYLIB_H is
// already defined.
#include <raylib.h>
#include <raymath.h>

#include "render_components.h"
#include "transform.h"

inline void DrawBoxRenderables(entt::registry &registry) {
    auto view = registry.view<WorldTransform, BoxRenderable>();
    for (auto entity : view) {
        auto [transform, box] = view.get<WorldTransform, BoxRenderable>(entity);
        Vector3 position = Vector3Transform(Vector3Zero(), transform.matrix);
        DrawCubeV(position, box.size, box.color);
        DrawCubeWiresV(position, box.size, DARKBLUE);
    }
}

#endif // SCENE_RENDERER_H
