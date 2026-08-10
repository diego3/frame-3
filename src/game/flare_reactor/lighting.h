// Lighting: a small Blinn-Phong-ish per-pixel lighting setup, ported from raylib's own
// "shaders_basic_lighting" example (vendor/raylib/examples/shaders/shaders_basic_lighting.c +
// resources/shaders/glsl330/lighting.vs/.fs) -- one directional "sun" (matching the skybox's own
// sunset mood) and one point light near the reactor's cyan core.
//
// Deliberately does NOT vendor raylib's rlights.h (also in that example directory): its
// CreateLight() increments a process-wide `static int lightsCount` shared across every Shader
// instance that ever calls it, capped at MAX_LIGHTS(4) *total*, not per-shader -- fine for a single
// shader instance (the example's own use case) but wrong here, where SetupLights (below) applies
// the exact same 2-light setup to N independently-compiled Shader instances (this class' own
// primitives shader, plus one fresh instance per reactor Model material via ApplyToModel) --
// rlights.h's shared counter would silently stop wiring up lights past the 4th independent call,
// with no error. SetupLights reimplements just the uniform-setting half (same "lights[i].field"
// naming lighting.fs expects) with no such shared state, safe to call on any number of shader
// instances.
//
// Two independently-compiled Shader instances per "thing lit" (GetPrimitivesMaterial() for every
// solid Box/Sphere Renderable, see app/scene/renderable.h; a fresh one per Model material via
// ApplyToModel) rather than one shared instance everywhere, on purpose: raylib's UnloadModel ->
// UnloadMaterial (vendor/raylib/src/rmodels.c) unconditionally UnloadShader's a non-default
// material shader while tearing the model down, with no refcounting across materials that happen
// to share the same Shader{id, locs} value -- sharing one canonical instance across the reactor's
// 12+1 materials would double-(and 12-times-over-)free the same GL program/locs pointer the moment
// that Model is ever unloaded (Engine::Shutdown(), via the Renderable::model shared_ptr's last
// release). Same reasoning game/flare_reactor/skybox.h documents for why its own shader isn't
// routed through Engine::GetShader()'s ResourceCache -- just generalized to "many owners, each an
// independent GL program" instead of "one owner".
#ifndef FLARE_REACTOR_LIGHTING_H
#define FLARE_REACTOR_LIGHTING_H

#include <memory>
#include <string>

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/resource/resource_cache.h"
#include "app/scene/material.h"

class Lighting {
public:
    // `textures`/`energyTexturePath`: GameConfig (game_config.h)'s energy texture asset, needed for
    // the scrolling core effect's texture (docs/learning/rendering.html, effect 2; lighting.fs's
    // `texture1`, see ApplyToModel below for why it's not a custom-named uniform) -- same "content
    // asset path threaded through main.cpp" shape skyboxCubemapPath/beaconSoundPath already have for
    // Skybox/FlareReactorView. Held as a ResourceCache<Texture2D> handle (ADR-0004) -- MUST outlive
    // this Lighting instance and be released before Engine::Shutdown(), same handle-lifetime
    // discipline every other cache handle in this project follows; main.cpp's g_lighting is a
    // unique_ptr reset before engine.Shutdown() for exactly this reason already.
    Lighting(ResourceCache<Texture2D> &textures, const std::string &energyTexturePath);
    ~Lighting();

    Lighting(const Lighting &) = delete;
    Lighting &operator=(const Lighting &) = delete;

    // The RenderMaterial (ADR-0019, app/scene/material.h) every solid (non-wireframe, non-Model)
    // Renderable is drawn with -- pass to app/scene/renderable.h's
    // DrawRenderables(registry, &lighting.GetPrimitivesMaterial()). Deliberately carries no rim
    // extras -- this is exactly the "generic solid Renderables (the Sentinel's Box/Sphere) shouldn't
    // get the reactor's rim glow" split ADR-0019 was written to make possible; before that ADR, the
    // primitives shader and every reactor Model material shared the same SetupRim call, so the
    // Sentinel picked up rim glow it was never meant to have.
    const RenderMaterial &GetPrimitivesMaterial() const { return primitivesMaterial_; }

    // Pushes the current camera position (needed for the shader's specular term) AND elapsed time
    // (needed for the scrolling core effect's UV offset -- docs/learning/rendering.html, effect 2)
    // to this shader AND to every already-lit reactor Model's own per-material shader (see
    // ApplyToModel) -- call once per frame, before drawing. Both are genuinely per-frame values, so
    // they're pushed directly here rather than through RenderMaterial::extras/ApplyExtras (which is
    // for values set once at material-creation time, see ApplyToModel below).
    void Update(entt::registry &registry, Vector3 viewPos) const;

    // Compiles one independent Shader instance per material (see the header comment on why one-per-
    // material, not shared), applies the rim glow + scrolling-core-texture extras (ADR-0019,
    // scrollSpeed/energyIntensity -- NOT the texture itself, see this .cpp's own comment on why) to
    // each, assigns the energy texture directly onto that submaterial's raylib Material.maps[], and
    // assigns the compiled shader to model.materials[i].shader for every i -- called once, right
    // after a "Renderable" component loader resolves shape == Model (main.cpp). The extras are
    // applied once here, not re-applied per frame/per draw -- they're static values, unlike `time`;
    // app/scene/renderable.h's Model draw path passes an empty extras list for exactly this reason.
    void ApplyToModel(Model &model) const;

private:
    Shader shader_;
    RenderMaterial primitivesMaterial_;
    std::shared_ptr<Texture2D> energyTexture_;
};

#endif // FLARE_REACTOR_LIGHTING_H
