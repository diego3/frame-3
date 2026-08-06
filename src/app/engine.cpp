#include "engine.h"

#include "hierarchy.h"
#include "raylib.h"

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

namespace {
    // Shared with the Run()/TickAndUpdateDraw namespace block below -- declared up here so
    // Init() can also set it (see Engine::Current()'s comment in engine.h for why this exists).
    Engine *g_runningEngine = nullptr;
}

Engine *Engine::Current() { return g_runningEngine; }

bool Engine::Init(const EngineConfig &config, const char *title) {
    config_ = config;
    g_runningEngine = this;

    InitWindow(config_.screenWidth, config_.screenHeight, title);

    InitAudioDevice();

    return IsWindowReady();
}

namespace {
    // emscripten_set_main_loop only accepts a plain function pointer, so on PLATFORM_WEB there's
    // no way to pass `this` through to the per-frame tick -- g_runningEngine (declared above,
    // set by both Init() and Run()) gives the trampoline below a way to reach the running Engine.
    void (*g_updateAndDraw)(void) = nullptr;

    // Dispatches queued events (ADR-0005), advances attached processes by the frame's delta time
    // (Ch. 4), then recomputes every entity's WorldTransform from the (possibly just-updated)
    // hierarchy (docs/adr/0002) -- all before handing off to the screen's own update/draw, on both
    // desktop and web.
    void TickAndUpdateDraw() {
        float dt = GetFrameTime();

        // Frame budget SLO: both loop paths in Run() below target config.targetFps (ADR-0011;
        // SetTargetFPS on desktop, emscripten_set_main_loop's rate argument on web), so a frame is
        // "in budget" under roughly 1000/targetFps ms.
        float dtMs = dt * 1000.0f;
        float frameBudgetMs = 1000.0f / static_cast<float>(g_runningEngine->Config().targetFps);
        if (dtMs > frameBudgetMs) {
            TraceLog(LOG_WARNING, "Frame budget exceeded: %.2fms (budget %.2fms)", dtMs, frameBudgetMs);
        }

        g_runningEngine->Events().DispatchQueued();
        g_runningEngine->Processes().Update(dt);
        PropagateTransforms(g_runningEngine->Registry());
        g_updateAndDraw();
    }
}

void Engine::Run(void (*updateAndDraw)(void)) {
    g_runningEngine = this;
    g_updateAndDraw = updateAndDraw;

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(TickAndUpdateDraw, config_.targetFps, 1);
#else
    SetTargetFPS(config_.targetFps);

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        TickAndUpdateDraw();
    }
#endif
}

void Engine::Shutdown() {
    // Engine holds no cache handle itself (ADR-0014 moved the one former exception, font/sound,
    // out to the game's own main()) -- so there's no live shared_ptr here for Clear() to race
    // against. Cleared anyway: any caller who *is* still holding one of their own handles at this
    // point keeps it working exactly as before (Clear() never force-unloads), this just drops the
    // caches' own now-pointless bookkeeping before the GL/audio context it describes goes away.
    // Every caller-held handle (Fonts()/Sounds()/Models()/Textures()/GetShader()) MUST already be
    // released by this point -- see the WARNING on Models()/Textures() in engine.h.
    fontCache_.Clear();
    soundCache_.Clear();
    modelCache_.Clear();
    textureCache_.Clear();
    shaderCache_.Clear();

    CloseAudioDevice();     // Close audio context

    CloseWindow();          // Close window and OpenGL context
}
