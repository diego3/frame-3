// Skybox: draws a full-sphere backdrop by texturing the inside of a unit cube with a cubemap
// loaded directly from a pre-cut cross/strip-layout image (raylib's own
// CUBEMAP_LAYOUT_AUTO_DETECT -- vendor/raylib/src/raylib.h). Ported from raylib's own
// "models_skybox_rendering" example (vendor/raylib/examples/models/models_skybox_rendering.c),
// trimmed to its non-HDR `LoadTextureCubemap` path -- the user's own cubemap PNGs
// (assets/cubemaps/Cubemap_Sky_NN-512x512.png, 2048x1536 = a 4x3 cross of 512x512 faces) are
// already in that format, so no panorama->cubemap conversion step is needed (an earlier version
// of this file supported an equirectangular panorama instead, via a ported
// GenTextureCubemap/cubemap.vs+fs render-to-FBO step -- dropped once real cubemap assets showed
// up, since carrying both loading paths for a still-single-consumer experiment is speculative;
// resurrect from git history if a future panorama-only asset ever needs it).
//
// Lives in game/flare_reactor rather than app/scene/ since it's a single-game, single-consumer
// experiment so far; promote it alongside app/scene/renderable.h once a second game module
// actually wants one (same discipline app/scene/scene_renderer.h's own history documents for
// BoxRenderable).
//
// Deliberately owns its shader directly via raylib's LoadShader/UnloadShader, NOT through
// Engine::GetShader()'s ResourceCache<Shader> (app/resource/resource_cache.h): raylib's own
// UnloadModel -> UnloadMaterial (vendor/raylib/src/rmodels.c) unconditionally calls UnloadShader on
// a non-default material shader while tearing down the model, so a Skybox model whose material held
// a ResourceCache-issued shared_ptr<Shader> would get UnloadShader'd twice -- once by UnloadModel
// here, once more when the shared_ptr's own deleter later runs. The dedup ResourceCache exists for
// doesn't apply here anyway: exactly one Model ever uses this shader, for exactly as long as that
// Model exists -- so this instead mirrors raylib's own example and owns shader+cubemap+mesh as one
// unit, released together in the destructor.
#ifndef FLARE_REACTOR_SKYBOX_H
#define FLARE_REACTOR_SKYBOX_H

#include <string>

#include <raylib.h>

class Skybox {
public:
    // `cubemapImagePath` is resolved the same way every other "resources/..." path in this
    // codebase is (relative to the executable's own directory, see engine.cpp's ChangeDirectory
    // comment) and must be a cross- or strip-layout cubemap image raylib's CUBEMAP_LAYOUT_AUTO_DETECT
    // can read (raylib.h) -- an equirectangular panorama is NOT this format, see the header comment.
    explicit Skybox(const std::string &cubemapImagePath);
    ~Skybox();

    Skybox(const Skybox &) = delete;
    Skybox &operator=(const Skybox &) = delete;

    // Must be called inside an open BeginMode3D/EndMode3D block, before any opaque geometry --
    // skybox.vs strips translation from the view matrix (so the cube always reads as infinitely far
    // regardless of camera position) but this still disables depth-mask writes around the draw call
    // so nothing later z-fights it, matching raylib's own example.
    void Draw() const;

private:
    Model model_{};
};

#endif // FLARE_REACTOR_SKYBOX_H
