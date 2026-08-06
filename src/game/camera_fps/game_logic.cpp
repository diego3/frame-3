#include "game_logic.h"

// raylib.h before raymath.h -- see game/camera_fps/human_view.cpp's own comment on this.
#include <raylib.h>
#include <raymath.h>

#include "app/transform.h"
#include "components.h"

namespace {
    // Movement constants, ported verbatim from
    // /home/diego/Documents/raylib/examples/core/core_3d_camera_fps.c's #defines -- the physics-
    // only half of what used to live in human_view.cpp (kSensitivity/kBottomHeight/kStandHeight/
    // kCrouchHeight stayed there -- camera presentation, not simulation).
    constexpr float kGravity = 32.0f;
    constexpr float kMaxSpeed = 20.0f;
    constexpr float kCrouchSpeed = 5.0f;
    constexpr float kJumpForce = 12.0f;
    constexpr float kMaxAccel = 150.0f;
    constexpr float kFriction = 0.86f;      // Grounded drag
    constexpr float kAirDrag = 0.98f;       // Increasing air drag increases strafing speed
    constexpr float kControl = 15.0f;       // Responsiveness turning movement dir to looked dir

    // Ported from the example's UpdateBody(), with GetFrameTime() replaced by the dt this project
    // already threads through every VOnUpdate (docs/adr/0010), the file-local `Body player`
    // replaced by an actor's own PlayerBody + LocalTransform, and `rot`/`side`/`forward`/
    // `jumpPressed`/`crouchHold` now read from that actor's own PlayerInput (docs/adr/0017 follow-
    // up) instead of being passed straight from a view's raw-input read.
    void UpdateBody(const PlayerInput &input, float dt, PlayerBody &body, LocalTransform &transform) {
        Vector2 moveInput = Vector2{static_cast<float>(input.side), static_cast<float>(-input.forward)};
        // NORMALIZE_INPUT left disabled, matching the example's own default.

        if (!body.isGrounded) body.velocity.y -= kGravity * dt;

        if (body.isGrounded && input.jumpPressed) {
            body.velocity.y = kJumpForce;
            body.isGrounded = false;
        }

        Vector3 front = Vector3{sinf(input.lookYaw), 0.0f, cosf(input.lookYaw)};
        Vector3 right = Vector3{cosf(-input.lookYaw), 0.0f, sinf(-input.lookYaw)};

        Vector3 desiredDir = Vector3{moveInput.x * right.x + moveInput.y * front.x, 0.0f,
                                      moveInput.x * right.z + moveInput.y * front.z};
        body.dir = Vector3Lerp(body.dir, desiredDir, kControl * dt);

        float decel = (body.isGrounded ? kFriction : kAirDrag);
        Vector3 hvel = Vector3{body.velocity.x * decel, 0.0f, body.velocity.z * decel};

        float hvelLength = Vector3Length(hvel);
        if (hvelLength < (kMaxSpeed * 0.01f)) hvel = Vector3{0.0f, 0.0f, 0.0f};

        // This is what creates strafing.
        float speed = Vector3DotProduct(hvel, body.dir);

        float maxSpeed = (input.crouchHold ? kCrouchSpeed : kMaxSpeed);
        float accel = Clamp(maxSpeed - speed, 0.0f, kMaxAccel * dt);
        hvel.x += body.dir.x * accel;
        hvel.z += body.dir.z * accel;

        body.velocity.x = hvel.x;
        body.velocity.z = hvel.z;

        transform.position.x += body.velocity.x * dt;
        transform.position.y += body.velocity.y * dt;
        transform.position.z += body.velocity.z * dt;

        // Fancy collision system against the floor.
        if (transform.position.y <= 0.0f) {
            transform.position.y = 0.0f;
            body.velocity.y = 0.0f;
            body.isGrounded = true;
        }
    }
}

void CameraFpsLogic::VOnUpdate(float dt) {
    auto view = registry_.view<PlayerInput, PlayerBody, LocalTransform>();
    for (auto entity : view) {
        auto [input, body, transform] = view.get<PlayerInput, PlayerBody, LocalTransform>(entity);
        UpdateBody(input, dt, body, transform);
    }

    BaseGameLogic::VOnUpdate(dt);
}
