// Input/key-binding system (docs/adr/0013): a data-driven action->key mapping seam between "what
// the player pressed" and "what the possessed actor does about it". Deliberately more advanced
// than Game Coding Complete's own reference input handling -- ../gamecode4's TeapotController and
// MovementController hardcode literal key characters (`if (c == 'W')`, `m_bKey['A']`) straight in
// the input handler, with no rebinding concept anywhere in the book's shipped code. This ADR is a
// conscious choice to go beyond that baseline, not a translation of it.
#ifndef INPUT_BINDINGS_H
#define INPUT_BINDINGS_H

#include <string>
#include <unordered_map>

#include <raylib.h>

// The set of actions a view understands is fixed code (ADR-0013 Decision 1: only *which key*
// triggers each one is data-driven, not the action set itself). Interact wasn't in the ADR's
// original sketch (movement only) -- added once a real consumer
// (docs/rfc/0001-flare-reactor-pipeline-experiment.md) needed a discrete, non-movement action.
enum class InputAction {
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    Interact,
};

class InputBindings {
public:
    using BindingMap = std::unordered_map<InputAction, int>;


    // Level-triggered (raylib IsKeyDown) -- true every frame the bound key is held. Right fit for
    // movement. Defined inline (not in input_bindings.cpp) deliberately: that .cpp is also
    // compiled into the test build (tests/input_bindings_test.cpp exercises the
    // LoadOrCreateInputBindings round trip), which never links -lraylib (same reasoning
    // src/Makefile already documents for game/sandbox/human_view.cpp). An unused inline function
    // never pulls in a symbol reference, so this stays test-build-safe as long as no test actually
    // calls it -- calling raylib's real IsKeyDown/IsKeyPressed still requires a window/GL context
    // to mean anything anyway, so nothing of value would be tested here regardless.
    bool IsDown(InputAction action) const {
        auto it = keys_.find(action);
        return it != keys_.end() && IsKeyDown(it->second);
    }

    // Edge-triggered (raylib IsKeyPressed) -- true only on the frame the bound key transitions
    // up->down. Right fit for a discrete one-shot action like Interact -- IsDown alone would fire
    // every frame the key stays held. Not in ADR-0013's original design (movement-only sketch);
    // added implementing it against a real consumer that needed a discrete action. Inline for the
    // same reason as IsDown above.
    bool IsPressed(InputAction action) const {
        auto it = keys_.find(action);
        return it != keys_.end() && IsKeyPressed(it->second);
    }

    // The raw raylib key code currently bound to action -- for inspection/tests, and the seam a
    // future rebinding UI would read to display the current binding (ADR-0013's own Tradeoffs
    // names that as future work, not built here). 0 (raylib's KEY_NULL) if action somehow has no
    // binding at all, which LoadOrCreateInputBindings never actually leaves unset. Not inline like
    // IsDown/IsPressed above -- it never calls a raylib runtime function, so it stays in
    // input_bindings.cpp with LoadOrCreateInputBindings, no test-link reason to special-case it.
    int KeyFor(InputAction action) const;

private:
    BindingMap keys_;
    friend InputBindings LoadOrCreateInputBindings(const std::string &path);
};

// Reads path; if it doesn't exist yet (first run), returns the defaults (matching HumanView's/
// FlareReactorView's pre-existing hardcoded scheme, so adopting this doesn't silently change
// behavior) and writes them out to that path -- same "first run creates the file" pattern
// LoadOrCreateEngineConfig (ADR-0011) already established. A binding missing from an existing file
// (e.g. written by an older build before a new InputAction existed) keeps its default.
InputBindings LoadOrCreateInputBindings(const std::string &path = "config/keybindings.yaml");

#endif // INPUT_BINDINGS_H
