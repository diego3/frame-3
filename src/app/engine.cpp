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

void Engine::Run(void (*updateAndDraw)(void)) {
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(updateAndDraw, 60, 1);
#else
    SetTargetFPS(60);       // Set our game to run at 60 frames-per-second

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        updateAndDraw();
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
