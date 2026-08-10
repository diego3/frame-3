#include "game_config.h"

#include <sstream>

#include "app/entity/entity_file_parser_yaml.h"
#include "app/io/file_io.h"

namespace {
    // Minimal, hand-written emitter, same shape as SerializeEngineConfig
    // (app/core/engine_config.cpp) -- scoped to GameConfig's flat, scalar-fields-only shape (key:
    // value lines, no nested maps/lists). Not a general EntityDefNode-to-YAML serializer: nothing
    // else needs that yet.
    std::string SerializeGameConfig(const GameConfig &config) {
        std::ostringstream out;
        out << "skyboxCubemapPath: " << config.skyboxCubemapPath << "\n";
        out << "beaconSoundPath: " << config.beaconSoundPath << "\n";
        out << "sentinelPatrolVoicePath: " << config.sentinelPatrolVoicePath << "\n";
        out << "sentinelInvestigateVoicePath: " << config.sentinelInvestigateVoicePath << "\n";
        return out.str();
    }

    // Shared by both the real load path (fallback == GameConfig{}, all-empty) and the first-run
    // seeding path (fallback == whatever defaultsPath's own parse already produced) -- either way, a
    // field missing/malformed in `contents` keeps whatever the caller passed as fallback rather than
    // failing the whole parse.
    GameConfig ParseGameConfig(const std::string &contents, const GameConfig &fallback) {
        YamlEntityFileParser parser;
        EntityDefNode root = parser.Parse(contents);

        GameConfig config = fallback;
        if (const EntityDefNode *v = root.TryGet("skyboxCubemapPath"))
            config.skyboxCubemapPath = v->AsString(config.skyboxCubemapPath);
        if (const EntityDefNode *v = root.TryGet("beaconSoundPath"))
            config.beaconSoundPath = v->AsString(config.beaconSoundPath);
        if (const EntityDefNode *v = root.TryGet("sentinelPatrolVoicePath"))
            config.sentinelPatrolVoicePath = v->AsString(config.sentinelPatrolVoicePath);
        if (const EntityDefNode *v = root.TryGet("sentinelInvestigateVoicePath"))
            config.sentinelInvestigateVoicePath = v->AsString(config.sentinelInvestigateVoicePath);
        return config;
    }
}

GameConfig LoadOrCreateGameConfig(const std::string &path, const std::string &defaultsPath) {
    std::string contents;
    if (TryReadWholeFile(path, contents)) {
        return ParseGameConfig(contents, GameConfig{});
    }

    // First run: config/game.yaml doesn't exist yet. Seed from the shipped, versioned defaultsPath
    // instead of a bare GameConfig{} (all-empty) struct literal -- falls back to that empty struct
    // only if even defaultsPath is missing, so a malformed/incomplete build still can't hard-fail
    // here (just ships with empty paths, which every GetHandle()/LoadImage() call downstream already
    // logs a TraceLog WARNING for rather than crashing).
    GameConfig defaults;
    std::string shippedDefaults;
    if (TryReadWholeFile(defaultsPath, shippedDefaults)) {
        defaults = ParseGameConfig(shippedDefaults, defaults);
    }

    WriteWholeFile(path, SerializeGameConfig(defaults));
    return defaults;
}
