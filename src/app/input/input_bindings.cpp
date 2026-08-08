#include "app/input/input_bindings.h"

#include <sstream>

#include <raylib.h>

#include "app/entity/entity_file_parser_yaml.h"
#include "app/io/file_io.h"

namespace {
    struct ActionName {
        InputAction action;
        const char *name;
    };

    // Order here is just serialization output order -- parsing (LoadOrCreateInputBindings) looks
    // each one up by name regardless of position.
    constexpr ActionName kActionNames[] = {
        {InputAction::MoveForward, "MoveForward"},
        {InputAction::MoveBackward, "MoveBackward"},
        {InputAction::MoveLeft, "MoveLeft"},
        {InputAction::MoveRight, "MoveRight"},
        {InputAction::Interact, "Interact"},
    };

    // Matches HumanView's/FlareReactorView's pre-existing hardcoded arrow-key scheme (and the
    // KEY_E this project's RFC-0001 already settled on for Interact) exactly, so adopting
    // InputBindings doesn't silently change behavior on first run.
    InputBindings::BindingMap DefaultBindings() {
        return {
            {InputAction::MoveForward, KEY_UP},
            {InputAction::MoveBackward, KEY_DOWN},
            {InputAction::MoveLeft, KEY_LEFT},
            {InputAction::MoveRight, KEY_RIGHT},
            {InputAction::Interact, KEY_E},
        };
    }

    // Minimal, hand-written emitter, same shape as engine_config.cpp's SerializeEngineConfig --
    // one "ActionName: <int>" line per binding, raw raylib key codes (ADR-0013's own Tradeoffs:
    // human-readable names are deferred until a rebinding UI actually needs to display one).
    std::string SerializeBindings(const InputBindings::BindingMap &keys) {
        std::ostringstream out;
        for (const ActionName &entry : kActionNames) {
            auto it = keys.find(entry.action);
            out << entry.name << ": " << (it != keys.end() ? it->second : 0) << "\n";
        }
        return out.str();
    }

    // Shared by both the real load path (fallback == DefaultBindings()) and the first-run seeding
    // path (fallback == whatever defaultsPath's own parse already produced) -- either way, an
    // action missing/malformed in `contents` keeps whatever the caller passed as fallback rather
    // than failing the whole parse.
    InputBindings::BindingMap ParseBindings(const std::string &contents, InputBindings::BindingMap fallback) {
        YamlEntityFileParser parser;
        EntityDefNode root = parser.Parse(contents);

        for (const ActionName &entry : kActionNames) {
            if (const EntityDefNode *v = root.TryGet(entry.name)) {
                fallback[entry.action] = v->AsInt(fallback[entry.action]);
            }
        }
        return fallback;
    }
}

int InputBindings::KeyFor(InputAction action) const {
    auto it = keys_.find(action);
    return it != keys_.end() ? it->second : 0;
}

InputBindings LoadOrCreateInputBindings(const std::string &path, const std::string &defaultsPath) {
    InputBindings bindings;

    std::string contents;
    if (TryReadWholeFile(path, contents)) {
        bindings.keys_ = ParseBindings(contents, DefaultBindings());
        return bindings;
    }

    // First run: config/keybindings.yaml doesn't exist yet. Seed from the shipped, versioned
    // defaultsPath (assets/config/keybindings.yaml, staged into resources/config/keybindings.yaml)
    // instead of the bare DefaultBindings() -- falls back to DefaultBindings() only if even
    // defaultsPath is missing, so a malformed/incomplete build still can't hard-fail here.
    bindings.keys_ = DefaultBindings();
    std::string shippedDefaults;
    if (TryReadWholeFile(defaultsPath, shippedDefaults)) {
        bindings.keys_ = ParseBindings(shippedDefaults, bindings.keys_);
    }

    WriteWholeFile(path, SerializeBindings(bindings.keys_));
    return bindings;
}
