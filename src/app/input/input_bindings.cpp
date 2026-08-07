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
    std::unordered_map<InputAction, int> DefaultBindings() {
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
    std::string SerializeBindings(const std::unordered_map<InputAction, int> &keys) {
        std::ostringstream out;
        for (const ActionName &entry : kActionNames) {
            auto it = keys.find(entry.action);
            out << entry.name << ": " << (it != keys.end() ? it->second : 0) << "\n";
        }
        return out.str();
    }
}

int InputBindings::KeyFor(InputAction action) const {
    auto it = keys_.find(action);
    return it != keys_.end() ? it->second : 0;
}

InputBindings LoadOrCreateInputBindings(const std::string &path) {
    InputBindings bindings;
    bindings.keys_ = DefaultBindings();   // fallback for any action missing/malformed below

    std::string contents;
    if (!TryReadWholeFile(path, contents)) {
        WriteWholeFile(path, SerializeBindings(bindings.keys_));
        return bindings;
    }

    YamlEntityFileParser parser;
    EntityDefNode root = parser.Parse(contents);

    for (const ActionName &entry : kActionNames) {
        if (const EntityDefNode *v = root.TryGet(entry.name)) {
            bindings.keys_[entry.action] = v->AsInt(bindings.keys_[entry.action]);
        }
    }

    return bindings;
}
