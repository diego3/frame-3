// DebugOverlay: an on-screen HUD reading live process stats straight from /proc/self, so the
// engine's own resource usage (not just FPS) is visible while developing it -- FPS answers "is
// the frame budget ok", this answers "why not" (CPU pegged? RSS growing? leaking file handles?).
// Linux desktop only (see debug_overlay.cpp) -- /proc doesn't exist on PLATFORM_WEB or non-Linux
// platforms, where every function below is a no-op.
#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

// Call once per frame, before drawing. Watches for the F3 toggle key and re-samples /proc/self at
// most once every sampling interval (see kSampleIntervalSec in the .cpp) -- reading those files is
// real syscall I/O, so it isn't done every frame.
void UpdateDebugOverlay(float dt);

// Draws the last sample in the top-left corner, if the overlay is currently toggled on. Must be
// called between BeginDrawing()/EndDrawing().
void DrawDebugOverlay();

#endif // DEBUG_OVERLAY_H
