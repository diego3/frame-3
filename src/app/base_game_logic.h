// Adapted BaseGameLogic (docs/adr/0010 Sec 2): activates the Logic/View split ADR-0001 Decision 2
// deferred and ADR-0009 reaffirmed "not yet." Owns a list of attached IGameViews and never knows
// or cares how many there are, what kind, or how they render/replicate/decide -- it just updates
// simulation state (via VLoadLevel/VOnUpdate) and lets views pull what they need from shared
// state or react to events pushed at them. That decoupling, not any particular method signature,
// is the point.
#ifndef BASE_GAME_LOGIC_H
#define BASE_GAME_LOGIC_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "event_manager.h"
#include "game_view.h"
#include "level_loader.h"
#include "process_manager.h"

// Trimmed to Loading/Running/Paused, not the book's fuller MainMenu/WaitingForPlayers/... states
// -- screens.h's GameScreen enum (LOGO/TITLE/OPTIONS/GAMEPLAY/ENDING) already answers the
// app-level "what screen" question those book states were partly covering; this state machine
// only needs to describe what happens *inside* the GAMEPLAY screen's own simulation.
enum class GameLogicState { Loading, Running, Paused };

class BaseGameLogic {
public:
    BaseGameLogic(entt::registry &registry, EventManager &events, ProcessManager &processes,
                  LevelLoader &levelLoader);
    virtual ~BaseGameLogic() = default;

    // Calls view->VOnAttach(id, actorId) before storing it -- a view is never held without having
    // been told its own id and (if any) possessed actor first.
    GameViewId AttachView(std::unique_ptr<IGameView> view,
                          std::optional<entt::entity> actorId = std::nullopt);
    void DetachView(GameViewId id);

    // Loads levelPath via levelLoader_ (ADR-0009); transitions Loading -> Running once it
    // returns. Virtual so a future game-specific BaseGameLogic subclass can add its own
    // post-load setup without this class needing to know about it. Returns every entity spawned,
    // in file order (forwarded from LevelLoader::Load) -- lets a caller identify "the player" as
    // spawned[0] (by the level file's own actors[] ordering) instead of guessing from registry
    // iteration order once a level has more than one entity (docs/adr/0017, game/camera_fps's
    // player + towers is the first level that actually needed this).
    virtual std::vector<entt::entity> VLoadLevel(const std::string &levelPath);

    // Ticks every attached view's VOnUpdate, unless Paused (the one piece of "what pausing
    // freezes" this class resolves for itself -- ADR-0010's own Open Questions left the rest,
    // e.g. whether ProcessManager should also freeze, undecided; Engine ticks ProcessManager
    // independently of BaseGameLogic and this doesn't touch that). Does NOT call VOnRender -- see
    // ADR-0010 Sec 3 for why rendering is deliberately not reachable through BaseGameLogic at all.
    // Virtual (docs/adr/0017 follow-up) so a game-specific subclass can run its own simulation step
    // around this one -- game/camera_fps/game_logic.h's CameraFpsLogic is the first: it advances
    // physics *then* calls this (via BaseGameLogic::VOnUpdate) to tick views, so a view renders
    // this frame's already-integrated world state.
    virtual void VOnUpdate(float dt);

    GameLogicState State() const { return state_; }

    // Minimal resolution of ADR-0010's own "pause behavior... isn't specified" Open Question:
    // Paused freezes view updates (see VOnUpdate), nothing else -- ProcessManager keeps running
    // regardless, since Engine ticks it independently of BaseGameLogic and this doesn't reach
    // into that. Calling Resume() before a level has ever loaded (state_ still Loading) will
    // incorrectly claim Running -- a known sharp edge of this being a minimal, not fully designed,
    // addition; revisit if pausing during a load screen ever becomes a real case.
    void Pause() { state_ = GameLogicState::Paused; }
    void Resume() { state_ = GameLogicState::Running; }

protected:
    entt::registry &registry_;
    EventManager &events_;
    ProcessManager &processes_;
    LevelLoader &levelLoader_;

private:
    std::vector<std::unique_ptr<IGameView>> views_;
    GameViewId nextViewId_ = 1;
    GameLogicState state_ = GameLogicState::Loading;
};

#endif // BASE_GAME_LOGIC_H
