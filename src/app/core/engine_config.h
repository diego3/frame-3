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

// Reads path; if it doesn't exist yet (first run), seeds it from defaultsPath instead of the bare
// EngineConfig{} struct literal -- defaultsPath is a shipped, versioned file (assets/config/
// engine.yaml, staged into resources/config/engine.yaml at build time), so a game-dev browsing the
// repo can see/edit the actual shipped defaults without reading C++. Falls back to EngineConfig{}
// only if even defaultsPath is missing (a malformed/incomplete build), so this never hard-fails.
// A field missing from either file (e.g. one written by an older build) keeps its struct default
// rather than failing the whole load -- forward/backward-compatible, the same philosophy
// EntityFactory::Create uses for an unrecognized component name.
EngineConfig LoadOrCreateEngineConfig(const std::string &path = "config/engine.yaml",
                                       const std::string &defaultsPath = "resources/config/engine.yaml");

#endif // ENGINE_CONFIG_H
