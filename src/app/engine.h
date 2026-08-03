// Application layer (Game Coding Complete Ch. 5): owns platform/window/audio lifecycle and
// drives the main loop. Deliberately does NOT own anything from the Game Logic+View layer
// (Ch. 9-10) -- the screen state machine (screens.h, screen_*.c) stays exactly as it is, still
// fused per-screen and driven by a plain function pointer passed to Run().
#ifndef ENGINE_H
#define ENGINE_H

#include <entt/entt.hpp>

#include "event_manager.h"
#include "process_manager.h"

class Engine {
public:
    // Opens the window, initializes the audio device, and loads the resources shared across all
    // screens (font, fxCoin -- see screens.h). Returns false if window creation failed.
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

private:
    entt::registry registry_;
    EventManager eventManager_;
    ProcessManager processManager_;
};

#endif // ENGINE_H
