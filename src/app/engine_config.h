// Engine-layer config (docs/adr/0011) -- exactly the parameters Engine::Init() already takes/uses
// (window size) or reads each frame (target FPS), plus fullscreen/master volume for Engine to grow
// into. Loaded once, before Engine::Init() opens the window (resolution/fullscreen have to be
// known before that call, same constraint the book's PlayerOptions.xml loading has).
#ifndef ENGINE_CONFIG_H
#define ENGINE_CONFIG_H

#include <string>

struct EngineConfig {
    int screenWidth = 800;
    int screenHeight = 450;
    bool fullscreen = false;
    int targetFps = 60;
    float masterVolume = 1.0f;
};

// Reads path; if it doesn't exist yet (first run), returns EngineConfig{} (the defaults above)
// and writes them out to that path, so the file exists and is human-editable for next time -- the
// same "first run creates PlayerOptions.xml" pattern the book uses. A field missing from an
// existing file (e.g. one written by an older build) keeps its struct default rather than failing
// the whole load -- forward/backward-compatible, the same philosophy EntityFactory::Create uses
// for an unrecognized component name.
EngineConfig LoadOrCreateEngineConfig(const std::string &path = "config/engine.yaml");

#endif // ENGINE_CONFIG_H
