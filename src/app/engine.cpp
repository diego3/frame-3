#include "engine.h"

#include "raylib.h"
#include "../game/screens.h"    // NOTE: font/fxCoin/music are declared here, shared with screens

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

bool Engine::Init(int screenWidth, int screenHeight, const char *title) {
    InitWindow(screenWidth, screenHeight, title);

    InitAudioDevice();

    // Load global data (assets that must be available in all screens, i.e. font)
    font = LoadFont("resources/characters/mecha.png");
    //music = LoadMusicStream("resources/audio/music/ambient.ogg"); // TODO: Load music
    fxCoin = LoadSound("resources/audio/fx/coin.wav");

    SetMusicVolume(music, 1.0f);
    PlayMusicStream(music);

    return IsWindowReady();
}

namespace {
    // emscripten_set_main_loop only accepts a plain function pointer, so on PLATFORM_WEB there's
    // no way to pass `this` through to the per-frame tick -- these globals give the trampoline
    // below a way to reach the running Engine. Only one Engine ever runs at a time (Run() is
    // called once from main()), so a pair of globals is simpler than reaching for a singleton.
    Engine *g_runningEngine = nullptr;
    void (*g_updateAndDraw)(void) = nullptr;

    // Frame budget SLO: both loop paths below target 60 FPS (SetTargetFPS(60) on desktop,
    // emscripten_set_main_loop(..., 60, 1) on web), so a frame is "in budget" under ~16.67ms.
    constexpr float kFrameBudgetMs = 1000.0f / 60.0f;

    // Advances attached processes by the frame's delta time (Ch. 4) before handing off to the
    // screen's own update/draw, on both desktop and web.
    void TickAndUpdateDraw() {
        float dt = GetFrameTime();

        float dtMs = dt * 1000.0f;
        if (dtMs > kFrameBudgetMs) {
            TraceLog(LOG_WARNING, "Frame budget exceeded: %.2fms (budget %.2fms)", dtMs, kFrameBudgetMs);
        }

        g_runningEngine->Processes().Update(dt);
        g_updateAndDraw();
    }
}

void Engine::Run(void (*updateAndDraw)(void)) {
    g_runningEngine = this;
    g_updateAndDraw = updateAndDraw;

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(TickAndUpdateDraw, 60, 1);
#else
    SetTargetFPS(60);       // Set our game to run at 60 frames-per-second

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        TickAndUpdateDraw();
    }
#endif
}

void Engine::Shutdown() {
    UnloadFont(font);
    UnloadMusicStream(music);
    UnloadSound(fxCoin);

    CloseAudioDevice();     // Close audio context

    CloseWindow();          // Close window and OpenGL context
}
