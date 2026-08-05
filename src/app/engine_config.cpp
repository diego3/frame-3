#include "engine_config.h"

#include <sstream>

#include "entity_file_parser_yaml.h"
#include "file_io.h"

namespace {
    // Minimal, hand-written emitter scoped to exactly EngineConfig's flat, scalar-fields-only
    // shape (key: value lines, no nested maps/lists) -- ADR-0008's IEntityFileParser only ever
    // needs to *read*, so writing YAML back out is new here. Not a general EntityDefNode-to-YAML
    // serializer: nothing else needs that yet, and this covers the actual need.
    std::string SerializeEngineConfig(const EngineConfig &config) {
        std::ostringstream out;
        out << "screenWidth: " << config.screenWidth << "\n";
        out << "screenHeight: " << config.screenHeight << "\n";
        out << "fullscreen: " << (config.fullscreen ? "true" : "false") << "\n";
        out << "targetFps: " << config.targetFps << "\n";
        out << "masterVolume: " << config.masterVolume << "\n";
        return out.str();
    }
}

EngineConfig LoadOrCreateEngineConfig(const std::string &path) {
    std::string contents;
    if (!TryReadWholeFile(path, contents)) {
        EngineConfig defaults;
        WriteWholeFile(path, SerializeEngineConfig(defaults));
        return defaults;
    }

    YamlEntityFileParser parser;
    EntityDefNode root = parser.Parse(contents);

    EngineConfig config;   // struct defaults are the fallback for any field missing below.
    if (const EntityDefNode *v = root.TryGet("screenWidth")) config.screenWidth = v->AsInt(config.screenWidth);
    if (const EntityDefNode *v = root.TryGet("screenHeight")) config.screenHeight = v->AsInt(config.screenHeight);
    if (const EntityDefNode *v = root.TryGet("fullscreen")) config.fullscreen = v->AsBool(config.fullscreen);
    if (const EntityDefNode *v = root.TryGet("targetFps")) config.targetFps = v->AsInt(config.targetFps);
    if (const EntityDefNode *v = root.TryGet("masterVolume")) config.masterVolume = v->AsFloat(config.masterVolume);

    return config;
}
