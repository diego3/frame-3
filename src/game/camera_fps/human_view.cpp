#include "human_view.h"

#include <optional>

// raylib.h must come before raymath.h -- raymath.h only skips its own Vector2/Vector3/Matrix
// struct definitions (which would otherwise conflict with raylib.h's) once it sees RAYLIB_H
// already defined. human_view.h no longer pulls raylib.h in itself now that CameraFpsView holds
// no raylib types directly (docs/adr/0017 follow-up -- Camera3D moved onto FirstPersonCameraRig).
#include <raylib.h>
#include <raymath.h>

#include "app/debug_overlay_screen_element.h"
#include "app/scene_renderer.h"
#include "app/transform.h"
#include "components.h"

namespace {
    // View-local camera/presentation constants only now -- the movement-physics constants
    // (gravity, friction, air drag, acceleration curve) moved to game_logic.cpp alongside the
    // simulation they tune (docs/adr/0017 follow-up: CameraFpsLogic, a real BaseGameLogic
    // subclass, owns the physics step now -- this view only reads input and presents a camera).
    constexpr float kSensitivityX = 0.001f;
    constexpr float kSensitivityY = 0.001f;
    constexpr float kCrouchHeight = 0.0f;
    constexpr float kStandHeight = 1.0f;
    constexpr float kBottomHeight = 0.5f;

    // Ported verbatim from the example's UpdateCameraFPS(), operating on a FirstPersonCameraRig
    // (docs/adr/0017) instead of file-local globals + an out-parameter Camera*.
    void UpdateCameraFps(FirstPersonCameraRig &rig) {
        const Vector3 up = Vector3{0.0f, 1.0f, 0.0f};
        const Vector3 targetOffset = Vector3{0.0f, 0.0f, -1.0f};

        Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, rig.lookRotation.x);

        float maxAngleUp = Vector3Angle(up, yaw);
        maxAngleUp -= 0.001f;
        if (-(rig.lookRotation.y) > maxAngleUp) rig.lookRotation.y = -maxAngleUp;

        float maxAngleDown = Vector3Angle(Vector3Negate(up), yaw);
        maxAngleDown *= -1.0f;
        maxAngleDown += 0.001f;
        if (-(rig.lookRotation.y) < maxAngleDown) rig.lookRotation.y = -maxAngleDown;

        Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));

        float pitchAngle = -rig.lookRotation.y - rig.lean.y;
        pitchAngle = Clamp(pitchAngle, -PI / 2 + 0.0001f, PI / 2 - 0.0001f);
        Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

        float headSin = sinf(rig.headTimer * PI);
        float headCos = cosf(rig.headTimer * PI);
        const float stepRotation = 0.01f;
        rig.camera.up = Vector3RotateByAxisAngle(up, pitch, headSin * stepRotation + rig.lean.x);

        const float bobSide = 0.1f;
        const float bobUp = 0.15f;
        Vector3 bobbing = Vector3Scale(right, headSin * bobSide);
        bobbing.y = fabsf(headCos * bobUp);

        rig.camera.position = Vector3Add(rig.camera.position, Vector3Scale(bobbing, rig.walkLerp));
        rig.camera.target = Vector3Add(rig.camera.position, pitch);
    }

    // Draws the floor tiles + red sun -- ported verbatim from the example's DrawLevel(), minus
    // the towers (those are now BoxRenderable entities, drawn below in FpsScene::VOnRender -- see
    // assets/levels/camera_fps.yaml). Scene dressing, not actors, same category as
    // game/sandbox/human_view.cpp's own DrawGrid() call staying outside the ECS.
    void DrawGroundAndSky() {
        const int floorExtent = 25;
        const float tileSize = 5.0f;
        const Color tileColor1 = Color{150, 200, 200, 255};

        for (int y = -floorExtent; y < floorExtent; y++) {
            for (int x = -floorExtent; x < floorExtent; x++) {
                if ((y & 1) && (x & 1)) {
                    DrawPlane(Vector3{x * tileSize, 0.0f, y * tileSize}, Vector2{tileSize, tileSize}, tileColor1);
                } else if (!(y & 1) && !(x & 1)) {
                    DrawPlane(Vector3{x * tileSize, 0.0f, y * tileSize}, Vector2{tileSize, tileSize}, LIGHTGRAY);
                }
            }
        }

        DrawSphere(Vector3{300.0f, 300.0f, 0.0f}, 100.0f, Color{255, 0, 0, 255});
    }

    // First IScreenElement (mirrors game/sandbox/human_view.cpp's GameplayScene): the 3D pass.
    // Holds registry_ + a reference to CameraFpsView's possessedActor_ (stable for the view's
    // whole lifetime, same reasoning FpsHud already used) and re-fetches FirstPersonCameraRig
    // every VOnRender call -- never caches a pointer into it (see components.h). The towers
    // themselves are drawn by app/scene_renderer.h's DrawBoxRenderables -- a generic, scene-graph-
    // driven step shared with any other view that wants it, not hand-rolled here.
    class FpsScene : public IScreenElement {
    public:
        FpsScene(entt::registry &registry, const std::optional<entt::entity> &possessedActor)
            : registry_(registry), possessedActor_(possessedActor) {}

        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
            (void)dt;
            if (!possessedActor_.has_value()) return;
            const FirstPersonCameraRig *rig = registry_.try_get<FirstPersonCameraRig>(*possessedActor_);
            if (rig == nullptr) return;

            BeginMode3D(rig->camera);
            DrawGroundAndSky();
            DrawBoxRenderables(registry_);
            EndMode3D();
        }

        int VGetZOrder() const override { return zOrder_; }
        void VSetZOrder(int zOrder) override { zOrder_ = zOrder; }
        bool VIsVisible() const override { return visible_; }
        void VSetVisible(bool visible) override { visible_ = visible; }

    private:
        entt::registry &registry_;
        const std::optional<entt::entity> &possessedActor_;
        int zOrder_ = 0;
        bool visible_ = true;
    };

    // Second IScreenElement (mirrors GameplayHud): the "Camera controls:" info box + live velocity
    // readout, ported verbatim from the example's post-EndMode3D DrawText calls -- reading the
    // possessed actor's PlayerBody component. zOrder_ higher than FpsScene's default (0) so it
    // layers on top, same convention as GameplayHud.
    class FpsHud : public IScreenElement {
    public:
        FpsHud(entt::registry &registry, const std::optional<entt::entity> &possessedActor)
            : registry_(registry), possessedActor_(possessedActor) {}

        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
            (void)dt;
            DrawRectangle(5, 5, 330, 75, Fade(SKYBLUE, 0.5f));
            DrawRectangleLines(5, 5, 330, 75, BLUE);

            DrawText("Camera controls:", 15, 15, 10, BLACK);
            DrawText("- Move keys: W, A, S, D, Space, Left-Ctrl", 15, 30, 10, BLACK);
            DrawText("- Look around: arrow keys or mouse", 15, 45, 10, BLACK);

            Vector3 velocity = Vector3Zero();
            if (possessedActor_.has_value()) {
                if (const PlayerBody *body = registry_.try_get<PlayerBody>(*possessedActor_)) {
                    velocity = body->velocity;
                }
            }
            DrawText(TextFormat("- Velocity Len: (%06.3f)", Vector2Length(Vector2{velocity.x, velocity.z})),
                     15, 60, 10, BLACK);
        }

        int VGetZOrder() const override { return zOrder_; }
        void VSetZOrder(int zOrder) override { zOrder_ = zOrder; }
        bool VIsVisible() const override { return visible_; }
        void VSetVisible(bool visible) override { visible_ = visible; }

    private:
        entt::registry &registry_;
        const std::optional<entt::entity> &possessedActor_;
        int zOrder_ = 100;
        bool visible_ = true;
    };

}

// DebugOverlayScreenElement's zOrder_ (200, app/debug_overlay_screen_element.h) sits above
// FpsHud's (100), preserving the draw-last-on-top order game/sandbox/main.cpp's own
// UpdateDrawFrame already used (HUD, then debug overlay).
CameraFpsView::CameraFpsView(entt::registry &registry) : registry_(registry) {
    PushElement(std::make_unique<FpsScene>(registry_, possessedActor_));
    PushElement(std::make_unique<FpsHud>(registry_, possessedActor_));
    PushElement(std::make_unique<DebugOverlayScreenElement>());
}

// Seeds FirstPersonCameraRig onto the newly-possessed actor -- view/presentation setup, so it
// belongs here rather than in main.cpp (which still emplaces PlayerBody itself: simulation state
// the game logic owns, docs/adr/0017). get_or_emplace rather than emplace so re-attaching (e.g. a
// future DetachView/AttachView cycle onto the same actor) doesn't reset an existing rig.
void CameraFpsView::VOnAttach(GameViewId id, std::optional<entt::entity> actorId) {
    HumanViewBase::VOnAttach(id, actorId);
    if (!actorId.has_value()) return;

    FirstPersonCameraRig &rig = registry_.get_or_emplace<FirstPersonCameraRig>(*actorId);
    rig.camera.fovy = 60.0f;
    rig.camera.projection = CAMERA_PERSPECTIVE;

    LocalTransform *transform = registry_.try_get<LocalTransform>(*actorId);
    Vector3 playerPosition = (transform != nullptr) ? transform->position : Vector3Zero();
    rig.camera.position = Vector3{playerPosition.x, playerPosition.y + (kBottomHeight + rig.headLerp),
                                   playerPosition.z};

    UpdateCameraFps(rig);   // Matches the example's own pre-loop UpdateCameraFPS(&camera) call.
}

void CameraFpsView::VOnUpdate(float dt) {
    UpdateElements(dt);

    if (!possessedActor_.has_value()) return;

    PlayerBody *body = registry_.try_get<PlayerBody>(*possessedActor_);
    LocalTransform *transform = registry_.try_get<LocalTransform>(*possessedActor_);
    FirstPersonCameraRig *rig = registry_.try_get<FirstPersonCameraRig>(*possessedActor_);
    if (body == nullptr || transform == nullptr || rig == nullptr) return;

    Vector2 mouseDelta = GetMouseDelta();
    rig->lookRotation.x -= mouseDelta.x * kSensitivityX;
    rig->lookRotation.y += mouseDelta.y * kSensitivityY;

    char sideway = static_cast<char>(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
    char forward = static_cast<char>(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
    bool crouching = IsKeyDown(KEY_LEFT_CONTROL);

    // Publishes this frame's input as intent for CameraFpsLogic::VOnUpdate to simulate on its
    // *next* tick (components.h explains the one-frame lag this implies, and why lookYaw is
    // copied here rather than CameraFpsLogic reading FirstPersonCameraRig directly).
    registry_.get_or_emplace<PlayerInput>(*possessedActor_) =
        PlayerInput{rig->lookRotation.x, sideway, forward, IsKeyPressed(KEY_SPACE), crouching};

    rig->headLerp = Lerp(rig->headLerp, (crouching ? kCrouchHeight : kStandHeight), 20.0f * dt);
    rig->camera.position = Vector3{transform->position.x, transform->position.y + (kBottomHeight + rig->headLerp),
                                    transform->position.z};

    if (body->isGrounded && ((forward != 0) || (sideway != 0))) {
        rig->headTimer += dt * 3.0f;
        rig->walkLerp = Lerp(rig->walkLerp, 1.0f, 10.0f * dt);
        rig->camera.fovy = Lerp(rig->camera.fovy, 55.0f, 5.0f * dt);
    } else {
        rig->walkLerp = Lerp(rig->walkLerp, 0.0f, 10.0f * dt);
        rig->camera.fovy = Lerp(rig->camera.fovy, 60.0f, 5.0f * dt);
    }

    rig->lean.x = Lerp(rig->lean.x, sideway * 0.02f, 10.0f * dt);
    rig->lean.y = Lerp(rig->lean.y, forward * 0.015f, 10.0f * dt);

    UpdateCameraFps(*rig);
}
