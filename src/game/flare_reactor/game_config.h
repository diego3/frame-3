// Game-layer config (docs/adr/0011) -- owned by flare_reactor, NOT by Engine, same reasoning
// game/sandbox/game_config.h documents. First real fields: the skybox/audio paths that used to be
// hardcoded directly in human_view.cpp/ai_view.h -- which cubemap, which sound/voice line to play
// is a game content choice, so it belongs here, not fused into the view/AI code that plays them.
//
// No hardcoded path literals in this struct on purpose (unlike game/sandbox/game_config.h's own
// characterTexturePath/coinSoundPath field defaults) -- the actual default values live only in
// assets/config/flare_reactor/game.yaml (shipped, versioned, staged into
// resources/config/flare_reactor/game.yaml at build time), so changing a shipped default is a YAML
// edit, never a C++ recompile. Mirrors EngineConfig/InputBindings' own defaultsPath mechanism
// (engine_config.h, ADR-0011's 2026-08-07 addendum) rather than sandbox's older bare-struct-literal
// GameConfig -- this is flare_reactor's first GameConfig, built after that addendum landed.
//
// Deliberately scoped to *content asset* paths (images, audio) -- NOT the skybox shader paths
// (skybox.cpp's own GLSL_VERSION-templated LoadShader calls) or the level path (main.cpp's
// VLoadLevel call): a shader is part of how the skybox is drawn, not swappable content the same way
// a cubemap image is; the level path is "which level", a different axis than "which asset" this
// struct is about. Revisit only if a real need to swap either shows up.
#ifndef FLARE_REACTOR_GAME_CONFIG_H
#define FLARE_REACTOR_GAME_CONFIG_H

#include <string>

struct GameConfig {
    std::string skyboxCubemapPath;
    std::string beaconSoundPath;
    std::string sentinelPatrolVoicePath;
    std::string sentinelInvestigateVoicePath;
};

// Reads path; if it doesn't exist yet (first run), seeds it from defaultsPath (assets/config/
// flare_reactor/game.yaml, staged into resources/config/flare_reactor/game.yaml) instead of a bare
// GameConfig{} struct literal -- see the header comment above. Only falls back to an empty
// GameConfig{} (every field "") if even defaultsPath is missing (a malformed/incomplete build), so
// this never hard-fails; a field missing from either file keeps whatever fallback was already in
// play, same forward/backward-compatible philosophy EntityFactory::Create uses for an unrecognized
// component name.
GameConfig LoadOrCreateGameConfig(const std::string &path = "config/game.yaml",
                                   const std::string &defaultsPath = "resources/config/flare_reactor/game.yaml");

#endif // FLARE_REACTOR_GAME_CONFIG_H
