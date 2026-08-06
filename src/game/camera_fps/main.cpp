// Entry point for game/camera_fps -- raylib's examples/core/core_3d_camera_fps.c ("raylib [core]
// example - 3d camera fps") ported in as frame-3's second concrete game module (docs/adr/0017).
// No screens.h-style multi-screen state machine here (the original example has none either --
// game/sandbox's Logo/Title/Options/Ending machinery is sandbox content, not part of the
// IGameView/IScreenElement pattern being reused): this is one Engine-driven scene for the whole
// run. No GameConfig, no font/sound loading -- the example uses neither.

#include <raylib.h>

#include "app/debug_overlay.h"
#include "app/engine.h"
#include "app/engine_config.h"
#include "human_view.h"

namespace {
    CameraFpsView *g_view = nullptr;

    void UpdateDrawFrame() {
        UpdateDebugOverlay(GetFrameTime());   // F3 toggles a /proc/self stats HUD (Linux desktop only)

        if (g_view) g_view->VOnUpdate(GetFrameTime());

        BeginDrawing();
            ClearBackground(RAYWHITE);
            if (g_view) g_view->VOnRender(GetFrameTime());
            DrawDebugOverlay();
        EndDrawing();
    }
}

int main() {
    Engine engine;
    if (!engine.Init(LoadOrCreateEngineConfig(), "raylib [core] example - 3d camera fps")) return 1;

    DisableCursor();   // Limit cursor to relative movement inside the window (mouse-look)

    CameraFpsView view;
    g_view = &view;
    view.VOnAttach(1, std::nullopt);   // No BaseGameLogic here to do this -- no ECS actor either.

    engine.Run(UpdateDrawFrame);

    g_view = nullptr;
    engine.Shutdown();
    return 0;
}
