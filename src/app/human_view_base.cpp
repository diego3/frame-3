#include "human_view_base.h"

#include <algorithm>

void HumanViewBase::VOnAttach(GameViewId id, std::optional<entt::entity> actorId) {
    id_ = id;
    possessedActor_ = actorId;
}

void HumanViewBase::UpdateElements(float dt) {
    for (auto &[id, element] : elements_) element->VOnUpdate(dt);
}

void HumanViewBase::VOnUpdate(float dt) {
    UpdateElements(dt);
}

void HumanViewBase::VOnRender(float dt) {
    // Sorted ascending by z-order right before rendering (mirrors GCC4::HumanView::VOnRender's own
    // m_ScreenElements.sort() pass) -- lower z-order renders first, so a higher z-order element
    // visually layers on top of one that rendered before it.
    std::stable_sort(elements_.begin(), elements_.end(), [](const auto &a, const auto &b) {
        return a.second->VGetZOrder() < b.second->VGetZOrder();
    });

    for (auto &[id, element] : elements_) {
        if (element->VIsVisible()) element->VOnRender(dt);
    }
}

ScreenElementId HumanViewBase::PushElement(std::unique_ptr<IScreenElement> element) {
    ScreenElementId id = nextElementId_++;
    elements_.emplace_back(id, std::move(element));
    return id;
}

void HumanViewBase::RemoveElement(ScreenElementId id) {
    std::erase_if(elements_, [id](const auto &pair) { return pair.first == id; });
}
