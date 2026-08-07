// Adapted IGameView (docs/adr/0010 Sec 1): the seam that lets BaseGameLogic stay ignorant of how
// many, or what kind, of views are attached -- a local HumanView, a networked RemoteView, a bot's
// AIView. Dropped from the book's IGameView, with no raylib/OpenGL analogue at this project's
// scope: VOnRestore()/device-loss recovery (a DirectX concept -- raylib/OpenGL exposes no
// equivalent hook to recover from), VOnMsgProc(AppMsg) (raylib already hands out polled input
// directly, no Win32 message-queue to translate from), and the book's two-arg
// VOnRender(gameTime, elapsedTime) (collapsed to a single dt -- nothing built so far needs
// absolute elapsed game time separately from the frame delta).
#ifndef GAME_VIEW_H
#define GAME_VIEW_H

#include <cstdint>
#include <optional>

#include <entt/entt.hpp>

enum class GameViewType { Human, Remote, AI, Other };
using GameViewId = std::uint32_t;

class IGameView {
public:
    virtual ~IGameView() = default;

    // Called once, when BaseGameLogic::AttachView adds this view. actorId is the entity this view
    // is bound to controlling/following, if any (e.g. a HumanView's player-controlled entity) --
    // std::optional because a RemoteView or an observer-only view may not possess an actor at
    // all. Concrete overrides are expected to set id_ = id themselves (it's protected, not set by
    // a base-class method) -- a small bit of repetition per view, matching the sketch this
    // interface was adapted from exactly.
    virtual void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) = 0;

    // Input + view-local state (camera, UI, network send/recv, AI decision-making -- whatever
    // this concrete view is for). Never touches rendering.
    virtual void VOnUpdate(float dt) = 0;

    // Rendering only, for views that do any (a RemoteView's VOnRender is typically a no-op).
    virtual void VOnRender(float dt) = 0;

    virtual GameViewType VGetType() const = 0;
    GameViewId GetId() const { return id_; }

protected:
    GameViewId id_ = 0;   // set by VOnAttach
};

#endif // GAME_VIEW_H
