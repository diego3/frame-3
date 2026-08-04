#include "engine.h"

#include "raylib.h"
#include "../game/screens.h"    // NOTE: font/fxCoin/music are declared here, shared with screens

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

Engine::Engine()
    : shaderCache_(
          [](const char *key) {
              auto [vsPath, fsPath] = ResourceCacheKeys::Split(key);
              return LoadShader(vsPath.empty() ? nullptr : vsPath.c_str(),
                                 fsPath.empty() ? nullptr : fsPath.c_str());
          },
          UnloadShader) {}

std::shared_ptr<Shader> Engine::GetShader(const std::string &vsPath, const std::string &fsPath) {
    return shaderCache_.GetHandle(ResourceCacheKeys::Combine(vsPath, fsPath));
}

bool Engine::Init(int screenWidth, int screenHeight, const char *title) {
    InitWindow(screenWidth, screenHeight, title);

    InitAudioDevice();

    // Load global data (assets that must be available in all screens, i.e. font) through the
    // resource caches (Ch. 8, ADR-0004) rather than calling LoadFont/LoadSound directly. screens.h's
    // screen_*.c files are still plain C and read font/fxCoin as plain extern globals (ADR-0001,
    // Decision 2), not through Engine or a shared_ptr -- so the cache's handle is kept alive here
    // (fontHandle_/soundHandle_) and the plain globals get a copy of the raylib value type, which
    // is how raylib itself expects Font/Sound to be passed around (a lightweight handle to
    // GPU/audio-resident data, not the data itself).
    fontHandle_ = fontCache_.GetHandle("resources/characters/mecha.png");
    font = *fontHandle_;
    //music = LoadMusicStream("resources/audio/music/ambient.ogg"); // TODO: Load music
    soundHandle_ = soundCache_.GetHandle("resources/audio/fx/coin.wav");
    fxCoin = *soundHandle_;

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
    // Release the cache handles (running UnloadFont/UnloadSound, via ResourceCache's own deleter)
    // before CloseAudioDevice()/CloseWindow() below tear down the contexts those Unload* calls
    // need to still be open -- reset explicitly here rather than left to whatever order Engine's
    // own members would otherwise be destroyed in, since that could run after this function
    // returns and the window/audio device are already closed.
    fontHandle_.reset();
    soundHandle_.reset();
    fontCache_.Clear();
    soundCache_.Clear();

    // modelCache_/textureCache_/shaderCache_ don't have an Engine-held handle the way
    // fontCache_/soundCache_ do (nothing loads a model/texture/shader at Init() time yet) -- so
    // there's no live shared_ptr here for Clear() to race against. Cleared anyway, for the same
    // reason: any caller who *is* still holding one of their handles at this point keeps it
    // working exactly as before (Clear() never force-unloads), this just drops the caches' own
    // now-pointless bookkeeping before the GL/audio context it describes goes away.
    modelCache_.Clear();
    textureCache_.Clear();
    shaderCache_.Clear();

    UnloadMusicStream(music);

    CloseAudioDevice();     // Close audio context

    CloseWindow();          // Close window and OpenGL context
}
