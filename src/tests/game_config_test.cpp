#include "doctest/doctest.h"

#include <filesystem>

#include "game/game_config.h"

namespace {
    const std::string kScratchDir = "test_scratch/game_config_test";
    const std::string kConfigPath = kScratchDir + "/game.yaml";
}

TEST_CASE("LoadOrCreateGameConfig creates the file on first run") {
    std::filesystem::remove_all(kScratchDir);

    LoadOrCreateGameConfig(kConfigPath);

    CHECK(std::filesystem::exists(kConfigPath));

    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("LoadOrCreateGameConfig does not fail when the file already exists") {
    std::filesystem::remove_all(kScratchDir);
    LoadOrCreateGameConfig(kConfigPath);

    CHECK_NOTHROW(LoadOrCreateGameConfig(kConfigPath));

    std::filesystem::remove_all(kScratchDir);
}
