#include "app/view/base_game_logic.h"

BaseGameLogic::BaseGameLogic(entt::registry &registry, EventManager &events,
                              ProcessManager &processes, LevelLoader &levelLoader)
    : registry_(registry), events_(events), processes_(processes), levelLoader_(levelLoader) {}

GameViewId BaseGameLogic::AttachView(std::unique_ptr<IGameView> view,
                                      std::optional<entt::entity> actorId) {
    GameViewId id = nextViewId_++;
    view->VOnAttach(id, actorId);
    views_.push_back(std::move(view));
    return id;
}

void BaseGameLogic::DetachView(GameViewId id) {
    std::erase_if(views_, [id](const std::unique_ptr<IGameView> &view) { return view->GetId() == id; });
}

std::vector<entt::entity> BaseGameLogic::VLoadLevel(const std::string &levelPath) {
    std::vector<entt::entity> spawned = levelLoader_.Load(registry_, events_, levelPath);
    state_ = GameLogicState::Running;
    return spawned;
}

void BaseGameLogic::VOnUpdate(float dt) {
    if (state_ == GameLogicState::Paused) return;

    for (auto &view : views_) view->VOnUpdate(dt);
}
