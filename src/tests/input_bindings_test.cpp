#include "doctest/doctest.h"

#include <filesystem>

#include <raylib.h>

#include "app/io/file_io.h"
#include "app/input/input_bindings.h"

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

TEST_CASE("LoadOrCreateInputBindings seeds first-run defaults from defaultsPath when present") {
    std::filesystem::remove_all(kScratchDir);
    const std::string defaultsPath = kScratchDir + "/shipped_defaults.yaml";
    // Deliberately every binding different from DefaultBindings()'s own KEY_UP/DOWN/LEFT/RIGHT/E,
    // so this can only pass if defaultsPath's values actually won -- not by coincidentally
    // matching them.
    WriteWholeFile(defaultsPath,
        "MoveForward: 87\n"    // 'W'
        "MoveBackward: 83\n"   // 'S'
        "MoveLeft: 65\n"       // 'A'
        "MoveRight: 68\n"      // 'D'
        "Interact: 32\n");     // KEY_SPACE

    InputBindings bindings = LoadOrCreateInputBindings(kBindingsPath, defaultsPath);

    CHECK(bindings.KeyFor(InputAction::MoveForward) == 87);
    CHECK(bindings.KeyFor(InputAction::MoveBackward) == 83);
    CHECK(bindings.KeyFor(InputAction::MoveLeft) == 65);
    CHECK(bindings.KeyFor(InputAction::MoveRight) == 68);
    CHECK(bindings.KeyFor(InputAction::Interact) == KEY_SPACE);
    CHECK(std::filesystem::exists(kBindingsPath));   // still written out, same as the no-defaultsPath case

    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("LoadOrCreateInputBindings falls back to DefaultBindings() when defaultsPath is also missing") {
    std::filesystem::remove_all(kScratchDir);

    InputBindings bindings = LoadOrCreateInputBindings(kBindingsPath, kScratchDir + "/does_not_exist.yaml");

    CHECK(bindings.KeyFor(InputAction::MoveForward) == KEY_UP);
    CHECK(bindings.KeyFor(InputAction::MoveBackward) == KEY_DOWN);
    CHECK(bindings.KeyFor(InputAction::MoveLeft) == KEY_LEFT);
    CHECK(bindings.KeyFor(InputAction::MoveRight) == KEY_RIGHT);
    CHECK(bindings.KeyFor(InputAction::Interact) == KEY_E);

    std::filesystem::remove_all(kScratchDir);
}
