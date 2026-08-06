#include "human_view.h"

#include <raymath.h>

#include "app/transform.h"

namespace {
    // Placeholder input scheme -- raw key/gesture to actor-action mapping is an open question
    // (ADR-0010's own Open Questions, still not designed here); this hardcodes arrow keys to X/Z
    // movement so there's something to observe, not a real bindings layer.
    constexpr float kMoveUnitsPerSecond = 4.0f;

    // First real IScreenElement (docs/adr/0016): wraps what used to be HumanView::VOnRender's
    // direct body -- same rendering, now reachable/orderable as an element instead of hardcoded.
    // Holds camera_ by reference to HumanView's own Camera3D member (updated each frame by
    // HumanView::VOnUpdate) -- safe for GameplayScene's whole lifetime, since it's owned by the
    // same HumanView instance whose member it points at.
    class GameplayScene : public IScreenElement {
    public:
        GameplayScene(entt::registry &registry, const Camera3D &camera)
            : registry_(registry), camera_(camera) {}

        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
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

        int VGetZOrder() const override { return zOrder_; }
        void VSetZOrder(int zOrder) override { zOrder_ = zOrder; }
        bool VIsVisible() const override { return visible_; }
        void VSetVisible(bool visible) override { visible_ = visible; }

    private:
        entt::registry &registry_;
        const Camera3D &camera_;
        int zOrder_ = 0;
        bool visible_ = true;
    };

    // Second real IScreenElement (docs/adr/0016): the "PRESS ENTER..." prompt used to live in
    // screen_gameplay.cpp's DrawGameplayScreen, drawn directly and entirely outside HumanView.
    // Moved here so a HUD element actually exercises z-ordering against GameplayScene, not just
    // single-element indirection through the same interface. zOrder_ deliberately higher than
    // GameplayScene's default (0) so it renders after the 3D pass, visually layered on top --
    // mirrors TeapotWarsHumanView pushing StandardHUD alongside the base scene.
    class GameplayHud : public IScreenElement {
    public:
        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
            (void)dt;
            DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 20, 20, MAROON);
        }

        int VGetZOrder() const override { return zOrder_; }
        void VSetZOrder(int zOrder) override { zOrder_ = zOrder; }
        bool VIsVisible() const override { return visible_; }
        void VSetVisible(bool visible) override { visible_ = visible; }

    private:
        int zOrder_ = 100;
        bool visible_ = true;
    };
}

HumanView::HumanView(entt::registry &registry, ProcessManager &processes, ResourceCache<Sound> &sounds)
    : registry_(registry), processes_(processes), sounds_(sounds) {
    camera_.position = Vector3{0.0f, 10.0f, 10.0f};
    camera_.target = Vector3{0.0f, 0.0f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    PushElement(std::make_unique<GameplayScene>(registry_, camera_));
    PushElement(std::make_unique<GameplayHud>());
}

void HumanView::VOnUpdate(float dt) {
    UpdateElements(dt);

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

// VOnRender/PushElement/RemoveElement/VOnAttach live in HumanViewBase (app/human_view_base.cpp)
// now -- nothing sandbox-specific about them.
