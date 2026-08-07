#include "game_config.h"

#include <sstream>

#include "app/entity/entity_file_parser_yaml.h"
#include "app/io/file_io.h"

namespace {
    // Minimal, hand-written emitter, same shape as SerializeEngineConfig (app/engine_config.cpp) --
    // scoped to GameConfig's flat, scalar-fields-only shape (key: value lines, no nested maps/
    // lists). Not a general EntityDefNode-to-YAML serializer: nothing else needs that yet.
    std::string SerializeGameConfig(const GameConfig &config) {
        std::ostringstream out;
        out << "characterTexturePath: " << config.characterTexturePath << "\n";
        out << "coinSoundPath: " << config.coinSoundPath << "\n";
        return out.str();
    }
}

GameConfig LoadOrCreateGameConfig(const std::string &path) {
    std::string contents;
    if (!TryReadWholeFile(path, contents)) {
        GameConfig defaults;
        WriteWholeFile(path, SerializeGameConfig(defaults));
        return defaults;
    }

    YamlEntityFileParser parser;
    EntityDefNode root = parser.Parse(contents);

    GameConfig config;   // struct defaults are the fallback for any field missing below.
    if (const EntityDefNode *v = root.TryGet("characterTexturePath"))
        config.characterTexturePath = v->AsString(config.characterTexturePath);
    if (const EntityDefNode *v = root.TryGet("coinSoundPath"))
        config.coinSoundPath = v->AsString(config.coinSoundPath);

    return config;
}
