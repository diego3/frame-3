// Application layer (Game Coding Complete Ch. 5): owns platform/window/audio lifecycle and
// drives the main loop. Deliberately does NOT own anything from the Game Logic+View layer
// (Ch. 9-10) -- the screen state machine (screens.h, screen_*.c) stays exactly as it is, still
// fused per-screen and driven by a plain function pointer passed to Run().
#ifndef ENGINE_H
#define ENGINE_H

#include <memory>

#include <entt/entt.hpp>
#include <raylib.h>

#include "event_manager.h"
#include "process_manager.h"
#include "resource_cache.h"

class Engine {
public:
    // shaderCache_ needs a lambda (to unfold LoadShader's two-path key, see ResourceCacheKeys),
    // not a plain function pointer -- that's the one member the default-member-initializer shape
    // below (fontCache_ etc.) can't express, so it's built in the constructor body instead.
    Engine();

    // Opens the window, initializes the audio device, and loads the resources shared across all
    // screens (font, fxCoin -- see screens.h) through fontCache_/soundCache_ (Ch. 8, ADR-0004).
    // Returns false if window creation failed.
    bool Init(int screenWidth, int screenHeight, const char *title);

    // Drives the main loop until the window should close, calling updateAndDraw() once per frame.
    // Branches internally on PLATFORM_WEB (emscripten_set_main_loop) vs. desktop (a plain while
    // loop) -- the platform-specific mechanics this is meant to hide from main(). Each frame, any
    // events Queue()'d since the last frame are dispatched (ADR-0005), then attached processes are
    // advanced by the frame's delta time (Ch. 4), both before updateAndDraw() runs -- so neither
    // deferred cross-system events nor multi-frame behavior (camera shake, timed effects) need to
    // live in the screen code.
    void Run(void (*updateAndDraw)(void));

    // Unwinds exactly what Init() set up, in reverse.
    void Shutdown();

    entt::registry &Registry() { return registry_; }
    EventManager &Events() { return eventManager_; }
    ProcessManager &Processes() { return processManager_; }
    ResourceCache<Font> &Fonts() { return fontCache_; }
    ResourceCache<Sound> &Sounds() { return soundCache_; }

    // WARNING: every handle GetHandle() on these two returns (directly, or via GetShader() below)
    // MUST be released -- reset(), go out of scope, or have its owning entity/component destroyed
    // -- before Shutdown() runs. Shutdown() closes the GL/audio context these resources' Unload*
    // calls need; a handle that outlives Shutdown() calls UnloadModel/UnloadTexture/UnloadShader
    // into an already-closed context when it's finally released, which is UB (reproduced as a
    // real SIGSEGV in rlUnloadShaderProgram while implementing this -- see ADR-0004's "What
    // actually shipped" section). fontHandle_/soundHandle_ below don't have this problem because
    // Engine holds and releases them itself, in the right order, inside Shutdown(); nothing plays
    // that role for whatever holds a Models()/Textures()/GetShader() handle, because that's
    // whichever gameplay code calls them, not Engine.
    ResourceCache<Model> &Models() { return modelCache_; }
    ResourceCache<Texture2D> &Textures() { return textureCache_; }

    // LoadShader takes a vertex *and* a fragment path, so ResourceCache<Shader> alone can't key on
    // a single `path` the way Models()/Textures() do (raylib also allows either path to be empty,
    // meaning "use the corresponding default shader" -- pass "" for vsPath and/or fsPath for that).
    // This wraps ResourceCacheKeys::Combine()/Split() so callers just pass both paths, same as
    // raylib's own LoadShader. Same handle-lifetime warning as Models()/Textures() above applies.
    std::shared_ptr<Shader> GetShader(const std::string &vsPath, const std::string &fsPath);

private:
    entt::registry registry_;
    EventManager eventManager_;
    ProcessManager processManager_;
    ResourceCache<Font> fontCache_{LoadFont, UnloadFont};
    ResourceCache<Sound> soundCache_{LoadSound, UnloadSound};
    // Complete 3D models (mesh + materials, including whatever textures a glTF/OBJ/IQM file
    // references -- raylib's model loaders pull those in automatically as part of LoadModel, not
    // through textureCache_ below) and standalone textures (applied independently of a loaded
    // model -- e.g. swapping a material's texture, a skybox, a UI icon) are different use cases,
    // hence two separate caches rather than routing both through one.
    ResourceCache<Model> modelCache_{LoadModel, UnloadModel};
    ResourceCache<Texture2D> textureCache_{LoadTexture, UnloadTexture};
    // Built in Engine's constructor (engine.cpp) -- see the Engine() comment above.
    ResourceCache<Shader> shaderCache_;

    // Keep Init()'s handles into fontCache_/soundCache_ alive for as long as Engine itself is
    // (font/fxCoin -- see screens.h -- are used everywhere, for the app's whole lifetime). Reset
    // in Shutdown(), *before* CloseAudioDevice()/CloseWindow() run, so UnloadFont/UnloadSound
    // (called by these handles' own deleters, per ResourceCache) never fire after the GL/audio
    // context they need is already gone -- see Shutdown()'s comment for why the order matters.
    std::shared_ptr<Font> fontHandle_;
    std::shared_ptr<Sound> soundHandle_;
};

#endif // ENGINE_H
