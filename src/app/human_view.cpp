#include "human_view.h"

#include <raymath.h>

#include "transform.h"

namespace {
    // Placeholder input scheme -- raw key/gesture to actor-action mapping is an open question
    // (ADR-0010's own Open Questions, still not designed here); this hardcodes arrow keys to X/Z
    // movement so there's something to observe, not a real bindings layer.
    constexpr float kMoveUnitsPerSecond = 4.0f;
}

HumanView::HumanView(entt::registry &registry) : registry_(registry) {
    camera_.position = Vector3{0.0f, 10.0f, 10.0f};
    camera_.target = Vector3{0.0f, 0.0f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void HumanView::VOnAttach(GameViewId id, std::optional<entt::entity> actorId) {
    id_ = id;
    possessedActor_ = actorId;
}

void HumanView::VOnUpdate(float dt) {
    if (!possessedActor_.has_value()) return;

    LocalTransform *transform = registry_.try_get<LocalTransform>(*possessedActor_);
    if (transform == nullptr) return;

    Vector3 move{0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_RIGHT)) move.x += 1.0f;
    if (IsKeyDown(KEY_LEFT))  move.x -= 1.0f;
    if (IsKeyDown(KEY_DOWN))  move.z += 1.0f;
    if (IsKeyDown(KEY_UP))    move.z -= 1.0f;

    transform->position = Vector3Add(transform->position, Vector3Scale(move, kMoveUnitsPerSecond * dt));

    // Camera trails the possessed actor, so movement is actually visible rather than drifting
    // off-frame.
    camera_.target = transform->position;
    camera_.position = Vector3Add(transform->position, Vector3{0.0f, 10.0f, 10.0f});
}

void HumanView::VOnRender(float dt) {
    (void)dt;

    BeginMode3D(camera_);

    auto view = registry_.view<WorldTransform>();
    for (auto entity : view) {
        Vector3 position = Vector3Transform(Vector3Zero(), view.get<WorldTransform>(entity).matrix);
        DrawCubeWires(position, 1.0f, 1.0f, 1.0f, MAROON);
    }

    DrawGrid(20, 1.0f);

    EndMode3D();
}
