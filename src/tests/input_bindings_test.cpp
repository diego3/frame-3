#include "doctest/doctest.h"

#include <filesystem>

#include <raylib.h>

#include "app/file_io.h"
#include "app/input_bindings.h"

namespace {
    // Isolated from the real config/ a running game would use, same pattern engine_config_test.cpp
    // already established.
    const std::string kScratchDir = "test_scratch/input_bindings_test";
    const std::string kBindingsPath = kScratchDir + "/keybindings.yaml";
}

TEST_CASE("LoadOrCreateInputBindings returns defaults and writes them out on first run") {
    std::filesystem::remove_all(kScratchDir);

    InputBindings bindings = LoadOrCreateInputBindings(kBindingsPath);

    CHECK(bindings.KeyFor(InputAction::MoveForward) == KEY_UP);
    CHECK(bindings.KeyFor(InputAction::MoveBackward) == KEY_DOWN);
    CHECK(bindings.KeyFor(InputAction::MoveLeft) == KEY_LEFT);
    CHECK(bindings.KeyFor(InputAction::MoveRight) == KEY_RIGHT);
    CHECK(bindings.KeyFor(InputAction::Interact) == KEY_E);
    CHECK(std::filesystem::exists(kBindingsPath));

    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("LoadOrCreateInputBindings reads existing values instead of overwriting them") {
    std::filesystem::remove_all(kScratchDir);
    WriteWholeFile(kBindingsPath,
        "MoveForward: 87\n"    // 'W'
        "MoveBackward: 83\n"   // 'S'
        "MoveLeft: 65\n"       // 'A'
        "MoveRight: 68\n"      // 'D'
        "Interact: 32\n");     // KEY_SPACE

    InputBindings bindings = LoadOrCreateInputBindings(kBindingsPath);

    CHECK(bindings.KeyFor(InputAction::MoveForward) == 87);
    CHECK(bindings.KeyFor(InputAction::MoveBackward) == 83);
    CHECK(bindings.KeyFor(InputAction::MoveLeft) == 65);
    CHECK(bindings.KeyFor(InputAction::MoveRight) == 68);
    CHECK(bindings.KeyFor(InputAction::Interact) == KEY_SPACE);

    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("LoadOrCreateInputBindings falls back to defaults for actions missing from an existing file") {
    std::filesystem::remove_all(kScratchDir);
    WriteWholeFile(kBindingsPath, "Interact: 32\n");   // only one action present -- e.g. an older build

    InputBindings bindings = LoadOrCreateInputBindings(kBindingsPath);

    CHECK(bindings.KeyFor(InputAction::Interact) == KEY_SPACE);
    CHECK(bindings.KeyFor(InputAction::MoveForward) == KEY_UP);   // default -- missing from the file

    std::filesystem::remove_all(kScratchDir);
}
