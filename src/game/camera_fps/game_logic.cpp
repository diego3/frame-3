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
    // `jumpPressed`/`crouchHold` now read from that actor's own MovementIntent (docs/adr/0017
    // follow-up) instead of being passed straight from a view's raw-input read. Returns whether
    // this call triggered a jump -- kept a pure function of its inputs/outputs (no EventManager
    // dependency of its own); the caller below queues EvtData_ActorJumped based on the result.
    bool UpdateBody(const MovementIntent &intent, float dt, PlayerBody &body, LocalTransform &transform) {
        Vector2 moveInput = Vector2{static_cast<float>(intent.side), static_cast<float>(-intent.forward)};
        // NORMALIZE_INPUT left disabled, matching the example's own default.

        if (!body.isGrounded) body.velocity.y -= kGravity * dt;

        bool jumped = false;
        if (body.isGrounded && intent.jumpPressed) {
            body.velocity.y = kJumpForce;
            body.isGrounded = false;
            jumped = true;
        }

        Vector3 front = Vector3{sinf(intent.facingYaw), 0.0f, cosf(intent.facingYaw)};
        Vector3 right = Vector3{cosf(-intent.facingYaw), 0.0f, sinf(-intent.facingYaw)};

        Vector3 desiredDir = Vector3{moveInput.x * right.x + moveInput.y * front.x, 0.0f,
                                      moveInput.x * right.z + moveInput.y * front.z};
        body.dir = Vector3Lerp(body.dir, desiredDir, kControl * dt);

        float decel = (body.isGrounded ? kFriction : kAirDrag);
        Vector3 hvel = Vector3{body.velocity.x * decel, 0.0f, body.velocity.z * decel};

        float hvelLength = Vector3Length(hvel);
        if (hvelLength < (kMaxSpeed * 0.01f)) hvel = Vector3{0.0f, 0.0f, 0.0f};

        // This is what creates strafing.
        float speed = Vector3DotProduct(hvel, body.dir);

        float maxSpeed = (intent.crouchHold ? kCrouchSpeed : kMaxSpeed);
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

        return jumped;
    }
}

void CameraFpsLogic::VOnUpdate(float dt) {
    auto view = registry_.view<MovementIntent, PlayerBody, LocalTransform>();
    for (auto entity : view) {
        auto [intent, body, transform] = view.get<MovementIntent, PlayerBody, LocalTransform>(entity);
        if (UpdateBody(intent, dt, body, transform)) {
            events_.Queue(EvtData_ActorJumped{entity});
        }
    }

    BaseGameLogic::VOnUpdate(dt);
}
