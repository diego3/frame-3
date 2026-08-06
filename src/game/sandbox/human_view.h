// The one concrete IGameView built now (docs/adr/0010 Sec 4). Reads raylib's polled input
// directly in VOnUpdate (Decision B in that section: no Win32-style message-proc translation
// layer -- raylib has no message-queue concept to translate from, IsKeyDown/etc. already *are*
// polled per-frame input state). Renders every entity that has a WorldTransform as a placeholder
// wireframe box -- a real render component (a Model handle via ResourceCache<Model>, ADR-0004)
// is still an open question (ADR-0010's own Open Questions); this doesn't invent one, it's just
// enough to make HumanView's camera/input loop observably real.
#ifndef HUMAN_VIEW_H
#define HUMAN_VIEW_H

#include <optional>

#include <entt/entt.hpp>
#include <raylib.h>

#include "app/game_view.h"
#include "app/process_manager.h"
#include "app/resource_cache.h"

class HumanView : public IGameView {
public:
    // processes/sounds: the "human" half of GCC4::HumanView's own dependencies (UserInterface/
    // HumanView.h -- m_pProcessManager "strictly for things like button animations, etc.",
    // InitAudio()) -- held the same way BaseGameLogic already holds a ProcessManager& it doesn't
    // call into yet (base_game_logic.h/.cpp): the seam a human-facing view needs (camera easing,
    // UI-timed effects, sound playback tied to view-level events) once one of those becomes a
    // concrete need, not before.
    HumanView(entt::registry &registry, ProcessManager &processes, ResourceCache<Sound> &sounds);

    void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) override;
    void VOnUpdate(float dt) override;
    void VOnRender(float dt) override;
    GameViewType VGetType() const override { return GameViewType::Human; }

private:
    entt::registry &registry_;
    ProcessManager &processes_;
    ResourceCache<Sound> &sounds_;
    std::optional<entt::entity> possessedActor_;
    Camera3D camera_;
};

#endif // HUMAN_VIEW_H
