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
    // Opens the window, initializes the audio device, and loads the resources shared across all
    // screens (font, fxCoin -- see screens.h) through fontCache_/soundCache_ (Ch. 8, ADR-0004).
    // Returns false if window creation failed.
    bool Init(int screenWidth, int screenHeight, const char *title);

    // Drives the main loop until the window should close, calling updateAndDraw() once per frame.
    // Branches internally on PLATFORM_WEB (emscripten_set_main_loop) vs. desktop (a plain while
    // loop) -- the platform-specific mechanics this is meant to hide from main(). Each frame,
    // attached processes are advanced by the frame's delta time (Ch. 4) before updateAndDraw()
    // runs, so multi-frame behavior (camera shake, timed effects) stays out of the screen code.
    void Run(void (*updateAndDraw)(void));

    // Unwinds exactly what Init() set up, in reverse.
    void Shutdown();

    entt::registry &Registry() { return registry_; }
    EventManager &Events() { return eventManager_; }
    ProcessManager &Processes() { return processManager_; }
    ResourceCache<Font> &Fonts() { return fontCache_; }
    ResourceCache<Sound> &Sounds() { return soundCache_; }

private:
    entt::registry registry_;
    EventManager eventManager_;
    ProcessManager processManager_;
    ResourceCache<Font> fontCache_{LoadFont, UnloadFont};
    ResourceCache<Sound> soundCache_{LoadSound, UnloadSound};

    // Keep Init()'s handles into fontCache_/soundCache_ alive for as long as Engine itself is
    // (font/fxCoin -- see screens.h -- are used everywhere, for the app's whole lifetime). Reset
    // in Shutdown(), *before* CloseAudioDevice()/CloseWindow() run, so UnloadFont/UnloadSound
    // (called by these handles' own deleters, per ResourceCache) never fire after the GL/audio
    // context they need is already gone -- see Shutdown()'s comment for why the order matters.
    std::shared_ptr<Font> fontHandle_;
    std::shared_ptr<Sound> soundHandle_;
};

#endif // ENGINE_H
