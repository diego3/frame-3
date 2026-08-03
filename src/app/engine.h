// Application layer (Game Coding Complete Ch. 5): owns platform/window/audio lifecycle and
// drives the main loop. Deliberately does NOT own anything from the Game Logic+View layer
// (Ch. 9-10) -- the screen state machine (screens.h, screen_*.c) stays exactly as it is, still
// fused per-screen and driven by a plain function pointer passed to Run().
#ifndef ENGINE_H
#define ENGINE_H

#include <entt/entt.hpp>

class Engine {
public:
    // Opens the window, initializes the audio device, and loads the resources shared across all
    // screens (font, fxCoin -- see screens.h). Returns false if window creation failed.
    bool Init(int screenWidth, int screenHeight, const char *title);

    // Drives the main loop until the window should close, calling updateAndDraw() once per frame.
    // Branches internally on PLATFORM_WEB (emscripten_set_main_loop) vs. desktop (a plain while
    // loop) -- the platform-specific mechanics this is meant to hide from main().
    void Run(void (*updateAndDraw)(void));

    // Unwinds exactly what Init() set up, in reverse.
    void Shutdown();

    entt::registry &Registry() { return registry_; }

private:
    entt::registry registry_;
};

#endif // ENGINE_H
