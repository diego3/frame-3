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

// The player actor's captured movement intent for the *next* physics step -- how
// CameraFpsView::VOnUpdate (raw input -> meaning) hands off to CameraFpsLogic::VOnUpdate
// (meaning -> simulation), game_logic.h's whole reason to exist (docs/adr/0017 follow-up: the
// physics used to run inside the view directly, which is exactly the coupling ADR-0010's
// Logic/View split exists to avoid). lookYaw duplicates FirstPersonCameraRig.lookRotation.x
// deliberately, rather than CameraFpsLogic reaching into a presentation-only component to read
// it -- movement direction genuinely depends on facing, but Logic doesn't need to know *how* that
// facing was produced (mouse+keyboard here; could be a gamepad, a network snapshot, or an AI
// decision for a future RemoteView/AIView, none of which should need a FirstPersonCameraRig to
// exist at all). Not the same thing as ADR-0013's (still-Proposed) InputAction/InputBindings --
// that's raw-key-to-action *mapping*; this is one specific game's already-resolved movement
// intent for one frame.
struct PlayerInput {
    float lookYaw = 0.0f;   // radians, matches FirstPersonCameraRig.lookRotation.x
    char side = 0;          // -1/0/1, A/D
    char forward = 0;       // -1/0/1, S/W
    bool jumpPressed = false;
    bool crouchHold = false;
};

#endif // CAMERA_FPS_COMPONENTS_H
