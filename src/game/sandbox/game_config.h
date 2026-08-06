// Game-layer config (docs/adr/0011) -- owned by whatever specific game runs on frame-3, NOT by
// Engine, mirroring exactly why Engine doesn't own the screen state machine either (ADR-0001
// Decision 2). First real fields: the resource paths main.cpp used to hardcode directly in its
// Engine::Fonts()/Sounds() calls (docs/adr/0014) -- which specific font/sound to load is a game
// choice, so it belongs in the game's own config, the same way EngineConfig's fields are exactly
// what Engine::Init()/Run() already take/use.
#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <string>

struct GameConfig {
    std::string characterTexturePath = "resources/characters/mecha.png";
    std::string coinSoundPath = "resources/audio/fx/coin.wav";
};

// Reads path; if it doesn't exist yet (first run), returns GameConfig{} (the defaults above) and
// writes them out to that path -- same "first run creates the file" pattern as
// LoadOrCreateEngineConfig. A field missing from an existing file (e.g. one written by an older
// build) keeps its struct default rather than failing the whole load -- same forward/backward-
// compatible philosophy EntityFactory::Create uses for an unrecognized component name.
GameConfig LoadOrCreateGameConfig(const std::string &path = "config/game.yaml");

#endif // GAME_CONFIG_H
