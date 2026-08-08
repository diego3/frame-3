#include "doctest/doctest.h"

#include <filesystem>

#include "app/core/engine_config.h"
#include "app/io/file_io.h"

namespace {
    // Isolated from the real config/ a running game would use (see .gitignore's
    // /src/test_scratch/ entry) -- these tests write/read real files, since
    // LoadOrCreateEngineConfig's whole job is that round-trip.
    const std::string kScratchDir = "test_scratch/engine_config_test";
    const std::string kConfigPath = kScratchDir + "/engine.yaml";
}

TEST_CASE("LoadOrCreateEngineConfig returns defaults and writes them out on first run") {
    std::filesystem::remove_all(kScratchDir);

    EngineConfig config = LoadOrCreateEngineConfig(kConfigPath);

    CHECK(config.screenWidth == 800);
    CHECK(config.screenHeight == 450);
    CHECK_FALSE(config.fullscreen);
    CHECK(config.targetFps == 60);
    CHECK(config.masterVolume == doctest::Approx(1.0f));
    CHECK(std::filesystem::exists(kConfigPath));

    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("LoadOrCreateEngineConfig reads existing values instead of overwriting them") {
    std::filesystem::remove_all(kScratchDir);
    WriteWholeFile(kConfigPath,
        "screenWidth: 1920\n"
        "screenHeight: 1080\n"
        "fullscreen: true\n"
        "targetFps: 144\n"
        "masterVolume: 0.5\n");

    EngineConfig config = LoadOrCreateEngineConfig(kConfigPath);

    CHECK(config.screenWidth == 1920);
    CHECK(config.screenHeight == 1080);
    CHECK(config.fullscreen);
    CHECK(config.targetFps == 144);
    CHECK(config.masterVolume == doctest::Approx(0.5f));

    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("LoadOrCreateEngineConfig falls back to defaults for fields missing from an existing file") {
    std::filesystem::remove_all(kScratchDir);
    WriteWholeFile(kConfigPath, "screenWidth: 1024\n");   // only one field present

    EngineConfig config = LoadOrCreateEngineConfig(kConfigPath);

    CHECK(config.screenWidth == 1024);
    CHECK(config.screenHeight == 450);   // default -- missing from the file

    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("LoadOrCreateEngineConfig seeds first-run defaults from defaultsPath when present") {
    std::filesystem::remove_all(kScratchDir);
    const std::string defaultsPath = kScratchDir + "/shipped_defaults.yaml";
    // Deliberately every field different from EngineConfig{}'s own struct literals, so this can
    // only pass if defaultsPath's values actually won -- not by coincidentally matching them.
    WriteWholeFile(defaultsPath,
        "screenWidth: 1280\n"
        "screenHeight: 720\n"
        "fullscreen: true\n"
        "targetFps: 144\n"
        "masterVolume: 0.75\n");

    EngineConfig config = LoadOrCreateEngineConfig(kConfigPath, defaultsPath);

    CHECK(config.screenWidth == 1280);
    CHECK(config.screenHeight == 720);
    CHECK(config.fullscreen);
    CHECK(config.targetFps == 144);
    CHECK(config.masterVolume == doctest::Approx(0.75f));
    CHECK(std::filesystem::exists(kConfigPath));   // still written out, same as the no-defaultsPath case

    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("LoadOrCreateEngineConfig falls back to EngineConfig{} when defaultsPath is also missing") {
    std::filesystem::remove_all(kScratchDir);

    EngineConfig config = LoadOrCreateEngineConfig(kConfigPath, kScratchDir + "/does_not_exist.yaml");

    CHECK(config.screenWidth == 800);
    CHECK(config.screenHeight == 450);
    CHECK_FALSE(config.fullscreen);
    CHECK(config.targetFps == 60);
    CHECK(config.masterVolume == doctest::Approx(1.0f));

    std::filesystem::remove_all(kScratchDir);
}
