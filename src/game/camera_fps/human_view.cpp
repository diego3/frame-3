#include "human_view.h"

#include <raymath.h>

namespace {
    // Movement/camera constants, ported verbatim from
    // /home/diego/Documents/raylib/examples/core/core_3d_camera_fps.c's #defines.
    constexpr float kSensitivityX = 0.001f;
    constexpr float kSensitivityY = 0.001f;
    constexpr float kGravity = 32.0f;
    constexpr float kMaxSpeed = 20.0f;
    constexpr float kCrouchSpeed = 5.0f;
    constexpr float kJumpForce = 12.0f;
    constexpr float kMaxAccel = 150.0f;
    constexpr float kFriction = 0.86f;      // Grounded drag
    constexpr float kAirDrag = 0.98f;       // Increasing air drag increases strafing speed
    constexpr float kControl = 15.0f;       // Responsiveness turning movement dir to looked dir
    constexpr float kCrouchHeight = 0.0f;
    constexpr float kStandHeight = 1.0f;
    constexpr float kBottomHeight = 0.5f;

    // Draws the level: floor tiles, 4 towers, a red sun -- ported verbatim from the example's
    // DrawLevel(). Entirely procedural/hardcoded geometry, no ECS entities or data-driven content
    // involved (docs/adr/0017 -- this is why EntityFactory/LevelLoader don't apply to this game).
    void DrawLevel() {
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

        const Vector3 towerSize = Vector3{16.0f, 32.0f, 16.0f};
        const Color towerColor = Color{150, 200, 200, 255};

        Vector3 towerPos = Vector3{16.0f, 16.0f, 16.0f};
        DrawCubeV(towerPos, towerSize, towerColor);
        DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

        towerPos.x *= -1;
        DrawCubeV(towerPos, towerSize, towerColor);
        DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

        towerPos.z *= -1;
        DrawCubeV(towerPos, towerSize, towerColor);
        DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

        towerPos.x *= -1;
        DrawCubeV(towerPos, towerSize, towerColor);
        DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

        DrawSphere(Vector3{300.0f, 300.0f, 0.0f}, 100.0f, Color{255, 0, 0, 255});
    }

    // First IScreenElement (mirrors game/sandbox/human_view.cpp's GameplayScene): the 3D pass.
    // Holds camera_ by reference to CameraFpsView's own Camera3D member, updated each frame by
    // CameraFpsView::VOnUpdate -- safe for FpsScene's whole lifetime, same reasoning as
    // GameplayScene's own camera_ reference.
    class FpsScene : public IScreenElement {
    public:
        explicit FpsScene(const Camera3D &camera) : camera_(camera) {}

        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
            (void)dt;
            BeginMode3D(camera_);
            DrawLevel();
            EndMode3D();
        }

        int VGetZOrder() const override { return zOrder_; }
        void VSetZOrder(int zOrder) override { zOrder_ = zOrder; }
        bool VIsVisible() const override { return visible_; }
        void VSetVisible(bool visible) override { visible_ = visible; }

    private:
        const Camera3D &camera_;
        int zOrder_ = 0;
        bool visible_ = true;
    };

    // Second IScreenElement (mirrors GameplayHud): the "Camera controls:" info box + live velocity
    // readout, ported verbatim from the example's post-EndMode3D DrawText calls. zOrder_ higher
    // than FpsScene's default (0) so it layers on top, same convention as GameplayHud.
    class FpsHud : public IScreenElement {
    public:
        explicit FpsHud(const CameraFpsBody &player) : player_(player) {}

        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
            (void)dt;
            DrawRectangle(5, 5, 330, 75, Fade(SKYBLUE, 0.5f));
            DrawRectangleLines(5, 5, 330, 75, BLUE);

            DrawText("Camera controls:", 15, 15, 10, BLACK);
            DrawText("- Move keys: W, A, S, D, Space, Left-Ctrl", 15, 30, 10, BLACK);
            DrawText("- Look around: arrow keys or mouse", 15, 45, 10, BLACK);
            DrawText(TextFormat("- Velocity Len: (%06.3f)",
                                 Vector2Length(Vector2{player_.velocity.x, player_.velocity.z})),
                     15, 60, 10, BLACK);
        }

        int VGetZOrder() const override { return zOrder_; }
        void VSetZOrder(int zOrder) override { zOrder_ = zOrder; }
        bool VIsVisible() const override { return visible_; }
        void VSetVisible(bool visible) override { visible_ = visible; }

    private:
        const CameraFpsBody &player_;
        int zOrder_ = 100;
        bool visible_ = true;
    };
}

CameraFpsView::CameraFpsView() {
    camera_.fovy = 60.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
    camera_.position = Vector3{player_.position.x, player_.position.y + (kBottomHeight + headLerp_),
                                player_.position.z};
    UpdateCameraFps();   // Matches the example's own pre-loop UpdateCameraFPS(&camera) call.

    PushElement(std::make_unique<FpsScene>(camera_));
    PushElement(std::make_unique<FpsHud>(player_));
}

void CameraFpsView::VOnUpdate(float dt) {
    UpdateElements(dt);

    Vector2 mouseDelta = GetMouseDelta();
    lookRotation_.x -= mouseDelta.x * kSensitivityX;
    lookRotation_.y += mouseDelta.y * kSensitivityY;

    char sideway = static_cast<char>(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
    char forward = static_cast<char>(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
    bool crouching = IsKeyDown(KEY_LEFT_CONTROL);
    UpdateBody(lookRotation_.x, dt, sideway, forward, IsKeyPressed(KEY_SPACE), crouching);

    headLerp_ = Lerp(headLerp_, (crouching ? kCrouchHeight : kStandHeight), 20.0f * dt);
    camera_.position = Vector3{player_.position.x, player_.position.y + (kBottomHeight + headLerp_),
                                player_.position.z};

    if (player_.isGrounded && ((forward != 0) || (sideway != 0))) {
        headTimer_ += dt * 3.0f;
        walkLerp_ = Lerp(walkLerp_, 1.0f, 10.0f * dt);
        camera_.fovy = Lerp(camera_.fovy, 55.0f, 5.0f * dt);
    } else {
        walkLerp_ = Lerp(walkLerp_, 0.0f, 10.0f * dt);
        camera_.fovy = Lerp(camera_.fovy, 60.0f, 5.0f * dt);
    }

    lean_.x = Lerp(lean_.x, sideway * 0.02f, 10.0f * dt);
    lean_.y = Lerp(lean_.y, forward * 0.015f, 10.0f * dt);

    UpdateCameraFps();
}

// Ported verbatim from the example's UpdateBody(), with GetFrameTime() replaced by the dt this
// project already threads through every VOnUpdate (docs/adr/0010) instead of re-querying it.
void CameraFpsView::UpdateBody(float rot, float dt, char side, char forward, bool jumpPressed,
                                bool crouchHold) {
    Vector2 input = Vector2{static_cast<float>(side), static_cast<float>(-forward)};
    // NORMALIZE_INPUT left disabled, matching the example's own default.

    if (!player_.isGrounded) player_.velocity.y -= kGravity * dt;

    if (player_.isGrounded && jumpPressed) {
        player_.velocity.y = kJumpForce;
        player_.isGrounded = false;
    }

    Vector3 front = Vector3{sinf(rot), 0.0f, cosf(rot)};
    Vector3 right = Vector3{cosf(-rot), 0.0f, sinf(-rot)};

    Vector3 desiredDir = Vector3{input.x * right.x + input.y * front.x, 0.0f,
                                  input.x * right.z + input.y * front.z};
    player_.dir = Vector3Lerp(player_.dir, desiredDir, kControl * dt);

    float decel = (player_.isGrounded ? kFriction : kAirDrag);
    Vector3 hvel = Vector3{player_.velocity.x * decel, 0.0f, player_.velocity.z * decel};

    float hvelLength = Vector3Length(hvel);
    if (hvelLength < (kMaxSpeed * 0.01f)) hvel = Vector3{0.0f, 0.0f, 0.0f};

    // This is what creates strafing.
    float speed = Vector3DotProduct(hvel, player_.dir);

    float maxSpeed = (crouchHold ? kCrouchSpeed : kMaxSpeed);
    float accel = Clamp(maxSpeed - speed, 0.0f, kMaxAccel * dt);
    hvel.x += player_.dir.x * accel;
    hvel.z += player_.dir.z * accel;

    player_.velocity.x = hvel.x;
    player_.velocity.z = hvel.z;

    player_.position.x += player_.velocity.x * dt;
    player_.position.y += player_.velocity.y * dt;
    player_.position.z += player_.velocity.z * dt;

    // Fancy collision system against the floor.
    if (player_.position.y <= 0.0f) {
        player_.position.y = 0.0f;
        player_.velocity.y = 0.0f;
        player_.isGrounded = true;
    }
}

// Ported verbatim from the example's UpdateCameraFPS().
void CameraFpsView::UpdateCameraFps() {
    const Vector3 up = Vector3{0.0f, 1.0f, 0.0f};
    const Vector3 targetOffset = Vector3{0.0f, 0.0f, -1.0f};

    Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, lookRotation_.x);

    float maxAngleUp = Vector3Angle(up, yaw);
    maxAngleUp -= 0.001f;
    if (-(lookRotation_.y) > maxAngleUp) lookRotation_.y = -maxAngleUp;

    float maxAngleDown = Vector3Angle(Vector3Negate(up), yaw);
    maxAngleDown *= -1.0f;
    maxAngleDown += 0.001f;
    if (-(lookRotation_.y) < maxAngleDown) lookRotation_.y = -maxAngleDown;

    Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));

    float pitchAngle = -lookRotation_.y - lean_.y;
    pitchAngle = Clamp(pitchAngle, -PI / 2 + 0.0001f, PI / 2 - 0.0001f);
    Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

    float headSin = sinf(headTimer_ * PI);
    float headCos = cosf(headTimer_ * PI);
    const float stepRotation = 0.01f;
    camera_.up = Vector3RotateByAxisAngle(up, pitch, headSin * stepRotation + lean_.x);

    const float bobSide = 0.1f;
    const float bobUp = 0.15f;
    Vector3 bobbing = Vector3Scale(right, headSin * bobSide);
    bobbing.y = fabsf(headCos * bobUp);

    camera_.position = Vector3Add(camera_.position, Vector3Scale(bobbing, walkLerp_));
    camera_.target = Vector3Add(camera_.position, pitch);
}
