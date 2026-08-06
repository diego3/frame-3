// Game-layer config (docs/adr/0011) -- owned by whatever specific game runs on frame-3, NOT by
// Engine, mirroring exactly why Engine doesn't own the screen state machine either (ADR-0001
// Decision 2). Deliberately near-empty: this raylib-template game has no game-specific settings
// yet (no difficulty, no gameplay toggles) -- this establishes the seam, not a schema nobody
// asked for. Header-only: with zero fields there's nothing to parse, so this doesn't need
// ADR-0008's YAML machinery the way EngineConfig (engine_config.h/.cpp) does.
#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <string>

#include "app/file_io.h"

struct GameConfig {
    // (nothing yet -- add fields here as a specific game needs them)
};

// Writes an (empty, for now) file on first run so it exists and is discoverable for whoever adds
// the first real field later -- same "first run creates the file" pattern as
// LoadOrCreateEngineConfig, just with nothing to actually read back yet.
inline GameConfig LoadOrCreateGameConfig(const std::string &path = "config/game.yaml") {
    std::string contents;
    if (!TryReadWholeFile(path, contents)) {
        WriteWholeFile(path, "# GameConfig has no fields yet -- see docs/adr/0011-engine-and-game-config.md\n");
    }
    return GameConfig{};
}

#endif // GAME_CONFIG_H
