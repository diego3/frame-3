// FPS body/movement state for the possessed player actor -- ported from the raylib example's
// file-local `Body` struct (docs/adr/0017) into a real ECS component instead of view-local state,
// so the player is an actual actor (spawned via LevelLoader, like game/sandbox's) and
// HumanViewBase's possessedActor_ means something here too. Position itself is NOT duplicated
// here -- it lives in the entity's own LocalTransform/WorldTransform (app/transform.h), the same
// spatial component every other entity in this project uses.
//
// Kept as a game/camera_fps/-local component, not promoted to app/, unlike BoxRenderable
// (app/render_components.h): the fields and the algorithm that drives them (gravity, friction,
// air drag, acceleration curve -- see human_view.cpp) are tuned specifically to this FPS movement
// scheme, not a generic "physics body" app/ has any other consumer for yet. That's ADR-0012's
// (still-Proposed) job to design, once a second game actually needs its own movement scheme too --
// not to guess ahead of it from one data point.
#ifndef CAMERA_FPS_COMPONENTS_H
#define CAMERA_FPS_COMPONENTS_H

#include <raylib.h>

struct PlayerBody {
    Vector3 velocity{0.0f, 0.0f, 0.0f};
    Vector3 dir{0.0f, 0.0f, 0.0f};
    bool isGrounded = false;
};

// First-person camera rig: the Camera3D itself plus the look/head-bob easing state that drives it
// each frame -- ported from the example's other file-local globals (lookRotation, headTimer,
// walkLerp, headLerp, lean), moved here (was CameraFpsView's own member state) once the actor
// already owned PlayerBody -- no reason the camera rig should be the one piece of "how this player
// currently behaves" left outside the ECS. Owned by CameraFpsView::VOnAttach (not main.cpp, unlike
// PlayerBody): it's view/presentation setup, not simulation the game logic needs to know about.
//
// IMPORTANT: entt may relocate a component pool's backing storage on any create/destroy of that
// same component type, so nothing may hold a pointer/reference into a FirstPersonCameraRig across
// frames -- human_view.cpp's CameraFpsView/FpsScene re-fetch it via
// registry.try_get<FirstPersonCameraRig>(actor) every call instead of caching it.
struct FirstPersonCameraRig {
    Camera3D camera{};
    Vector2 lookRotation{0.0f, 0.0f};
    float headTimer = 0.0f;
    float walkLerp = 0.0f;
    float headLerp = 1.0f;   // STAND_HEIGHT
    Vector2 lean{0.0f, 0.0f};
};

// An actor's captured movement intent for the *next* physics step -- how
// PlayerMovementElement::VOnUpdate (raw input -> meaning, human_view.cpp) hands off to
// CameraFpsLogic::VOnUpdate (meaning -> simulation), game_logic.h's whole reason to exist
// (docs/adr/0017 follow-up: the physics used to run inside the view directly, which is exactly the
// coupling ADR-0010's Logic/View split exists to avoid).
//
// Deliberately named/shaped so nothing about it is human-specific, despite its only producer today
// being a human-driven view: CameraFpsLogic's physics step only ever reads a MovementIntent off
// whatever actor has one -- it has no idea, and doesn't need one, whether a human, a future
// AIView's decision tree, or a network snapshot for a future RemoteView produced it. Swapping the
// player's controller for an AI later means writing an AIView that emplaces this same component
// with its own decided values; CameraFpsLogic doesn't change at all. facingYaw exists for the same
// reason: it's "which way this actor is currently facing, for the purpose of resolving movement
// direction relative to it" -- a fact about the actor, not the rendering camera -- even though its
// only current source happens to be FirstPersonCameraRig.lookRotation.x (deliberately copied here
// rather than CameraFpsLogic reaching into that presentation-only component to read it directly).
// Not the same thing as ADR-0013's (still-Proposed) InputAction/InputBindings -- that's
// raw-key-to-action *mapping*; this is one already-resolved movement intent for one frame.
struct MovementIntent {
    float facingYaw = 0.0f;   // radians -- which way the actor is currently facing
    char side = 0;            // -1/0/1, A/D
    char forward = 0;         // -1/0/1, S/W
    bool jumpPressed = false;
    bool crouchHold = false;
};

#endif // CAMERA_FPS_COMPONENTS_H
