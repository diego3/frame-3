#include "app/core/engine_config.h"

#include <sstream>

#include "app/entity/entity_file_parser_yaml.h"
#include "app/io/file_io.h"

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

    // Shared by both the real load path (fallback == EngineConfig{}) and the first-run seeding
    // path (fallback == whatever defaultsPath's own parse already produced) -- either way, a field
    // missing/malformed in `contents` keeps whatever the caller passed as fallback rather than
    // failing the whole parse.
    EngineConfig ParseEngineConfig(const std::string &contents, const EngineConfig &fallback) {
        YamlEntityFileParser parser;
        EntityDefNode root = parser.Parse(contents);

        EngineConfig config = fallback;
        if (const EntityDefNode *v = root.TryGet("screenWidth")) config.screenWidth = v->AsInt(config.screenWidth);
        if (const EntityDefNode *v = root.TryGet("screenHeight")) config.screenHeight = v->AsInt(config.screenHeight);
        if (const EntityDefNode *v = root.TryGet("fullscreen")) config.fullscreen = v->AsBool(config.fullscreen);
        if (const EntityDefNode *v = root.TryGet("targetFps")) config.targetFps = v->AsInt(config.targetFps);
        if (const EntityDefNode *v = root.TryGet("masterVolume")) config.masterVolume = v->AsFloat(config.masterVolume);
        return config;
    }
}

EngineConfig LoadOrCreateEngineConfig(const std::string &path, const std::string &defaultsPath) {
    std::string contents;
    if (TryReadWholeFile(path, contents)) {
        return ParseEngineConfig(contents, EngineConfig{});
    }

    // First run: config/engine.yaml doesn't exist yet. Seed from the shipped, versioned
    // defaultsPath (assets/config/engine.yaml, staged into resources/config/engine.yaml) instead
    // of the bare EngineConfig{} struct literal -- falls back to that struct literal only if even
    // defaultsPath is missing, so a malformed/incomplete build still can't hard-fail here.
    EngineConfig defaults;
    std::string shippedDefaults;
    if (TryReadWholeFile(defaultsPath, shippedDefaults)) {
        defaults = ParseEngineConfig(shippedDefaults, defaults);
    }

    WriteWholeFile(path, SerializeEngineConfig(defaults));
    return defaults;
}
