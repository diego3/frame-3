// Lighting: what's left, after ADR-0020's MeshRenderer/Light/.mat migration, of the small
// Blinn-Phong-ish per-pixel lighting setup ported from raylib's own "shaders_basic_lighting"
// example (vendor/raylib/examples/shaders/shaders_basic_lighting.c + resources/shaders/glsl330/
// lighting.vs/.fs). Everything that used to live here -- the hardcoded kLights[2] array, rim/
// scroll extras baked per reactor material, the scrolling core's energy texture -- moved to real
// ECS data: Light entities (app/scene/light.h), reactor_core.mat.yaml/reactor_frame.mat.yaml
// (app/scene/material_loader.h), and MeshRenderer (app/scene/mesh_renderer.h) respectively. See
// docs/adr/0020.
//
// What's left is just the ONE shader instance every solid (non-wireframe, non-MeshRenderer)
// Renderable -- the Sentinel's Box/Sphere -- draws with (app/scene/renderable.h's
// DrawRenderables), deliberately carrying no rim/scroll extras of its own (that split is exactly
// what ADR-0019 was written to make possible). Not routed through Engine::GetShader()'s
// ResourceCache<Shader> for the same reason MeshRenderer's own materials aren't yet (see
// app/scene/mesh_renderer.h's header comment) -- kept as its own independent compiled instance,
// unloaded by this class' own destructor.
#ifndef FLARE_REACTOR_LIGHTING_H
#define FLARE_REACTOR_LIGHTING_H

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/scene/material.h"

class Lighting {
public:
    Lighting();
    ~Lighting();

    Lighting(const Lighting &) = delete;
    Lighting &operator=(const Lighting &) = delete;

    // The RenderMaterial (ADR-0019, app/scene/material.h) every solid (non-wireframe, non-Model)
    // Renderable is drawn with -- pass to app/scene/renderable.h's
    // DrawRenderables(registry, &lighting.GetPrimitivesMaterial()). Deliberately carries no rim
    // extras -- generic solid Renderables (the Sentinel's Box/Sphere) shouldn't get the reactor
    // core's rim glow.
    const RenderMaterial &GetPrimitivesMaterial() const { return primitivesMaterial_; }

    // Pushes light.h's Frame uniforms (ambient/lights[]/viewPos/time, sourced from every
    // registry.view<WorldTransform, Light>() entity) to this shader AND to every MeshRenderer
    // entity's own shader (app/scene/mesh_renderer.h's UpdateMeshRendererFrameUniforms) -- call
    // once per frame, before drawing.
    void Update(entt::registry &registry, Vector3 viewPos) const;

private:
    Shader shader_;
    RenderMaterial primitivesMaterial_;
};

#endif // FLARE_REACTOR_LIGHTING_H
