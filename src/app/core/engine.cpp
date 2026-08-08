#include "app/core/engine.h"

#include "app/scene/hierarchy.h"
#include "raylib.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#if defined(PLATFORM_DESKTOP) && defined(__linux__)
    #include <cstdlib>
    #include <filesystem>
#endif

Engine::Engine()
    : shaderCache_(
          [](const char *key) {
              auto [vsPath, fsPath] = ResourceCacheKeys::Split(key);
              return LoadShader(vsPath.empty() ? nullptr : vsPath.c_str(),
                                 fsPath.empty() ? nullptr : fsPath.c_str());
          },
          UnloadShader) {
#if defined(PLATFORM_DESKTOP)
    // Every "resources/..." path in this codebase (level YAML, config defaults, audio, textures --
    // see engine_config.h/input_bindings.h/game_config.h/main.cpp of each game module) is relative,
    // resolved against the process's current working directory. build.sh's `resources` target only
    // ever stages assets into build/desktop/resources/ (src/Makefile), so those lookups only
    // resolve if the process happens to be launched from build/desktop -- which run.sh's own `cd
    // "$BUILD_DIR"` papers over, but running the binary directly (`./build/desktop/flare_reactor`
    // from anywhere else, a desktop launcher, gdb, ...) throws mini-yaml's "Cannot open file"
    // before a single frame renders. ChangeDirectory(GetApplicationDirectory()) makes relative
    // resource paths resolve next to the executable itself regardless of launch-time cwd; guarded
    // to desktop only since web/Android resolve "resources/..." through a bundled/preloaded
    // filesystem instead of a loose sibling directory next to the binary.
    //
    // Has to happen here, in the constructor, not in Init() below: LoadOrCreateEngineConfig()
    // (engine_config.h) already reads a relative default path (resources/config/engine.yaml)
    // *before* Init() ever runs, since every game's main() calls it as Init()'s own argument
    // (e.g. `engine.Init(LoadOrCreateEngineConfig(), ...)`) -- evaluated before Init()'s body, but
    // after `Engine engine;` has already constructed. The constructor is the only hook early enough
    // to cover it.
    ChangeDirectory(GetApplicationDirectory());
#endif

#if defined(PLATFORM_DESKTOP) && defined(__linux__)
    // Hybrid-graphics (Optimus) laptops in prime-select "on-demand" mode render on the low-power
    // Intel iGPU by default -- confirmed both via raylib's own startup log (TRACELOG prints
    // "Vendor: Intel") and by the process never showing up in `nvidia-smi`. run.sh's NVIDIA=1 flag
    // already opts a launch into __NV_PRIME_RENDER_OFFLOAD/__GLX_VENDOR_LIBRARY_NAME (see its own
    // comment), but that only helps when run.sh is doing the launching -- a binary started directly
    // (a debugger, a desktop launcher, a packaged release, CI) never goes through it. Mirror that
    // choice here as the default instead, but safely: only when glvnd actually has an NVIDIA vendor
    // registered (this exact JSON is how libglvnd resolves __GLX_VENDOR_LIBRARY_NAME=nvidia; its
    // absence means no NVIDIA driver is installed at all, e.g. CI or a non-hybrid box, where forcing
    // it would hard-fail GLX context creation instead of silently falling back), and only when
    // nothing has already expressed a preference -- an explicit env var, whether from run.sh's
    // NVIDIA=1, NVIDIA=0 opting back out, or a plain shell export, always wins over this default.
    if (!getenv("__GLX_VENDOR_LIBRARY_NAME") &&
        std::filesystem::exists("/usr/share/glvnd/egl_vendor.d/10_nvidia.json")) {
        setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 1);
        setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 1);
    }
#endif
}

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
        //float dtMs = dt * 1000.0f;
        //float frameBudgetMs = 1000.0f / static_cast<float>(g_runningEngine->Config().targetFps);
        //if (dtMs > frameBudgetMs) {
        //    TraceLog(LOG_WARNING, "Frame budget exceeded: %.2fms (budget %.2fms)", dtMs, frameBudgetMs);
        //}

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
    // registry_ itself can now hold a resource-cache handle inside a component (e.g.
    // Renderable::model, app/scene/renderable.h) -- unlike a caller's own local shared_ptr, nothing
    // external "releases" that one before this point; it only drops when its owning entity/
    // component is destroyed. Left alone, that destruction wouldn't happen until Engine's own
    // destructor runs (registry_ is a member, torn down after main()'s `Engine engine;` goes out of
    // scope -- i.e. after this whole function, and every game module's main() calls Shutdown()
    // before that point) -- by then CloseWindow()/CloseAudioDevice() below have already torn down
    // the context Unload* needs, so the eventual UnloadModel/UnloadTexture/etc. call fires into an
    // already-closed context (the exact SIGSEGV class ADR-0004 documents). Clearing registry_ here,
    // first, forces every component (and any resource-cache handle it holds) to be destroyed while
    // the context is still alive -- same "release before Shutdown()" contract every other handle
    // already had to follow (see the WARNING on Models()/Textures() in engine.h), just enforced
    // here instead of trusted to every future component.
    registry_.clear();

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
