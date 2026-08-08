#include "human_view.h"

#include <raymath.h>

#include "app/scene/renderable.h"
#include "app/scene/transform.h"

namespace {
    constexpr float kMoveUnitsPerSecond = 4.0f;

    // raylib has no positional/3D audio (PlaySound3D doesn't exist -- confirmed against
    // vendor/raylib/src/raylib.h; only PlaySound/SetSoundVolume/SetSoundPitch/SetSoundPan, a mono
    // pan). This is RFC-0001's "Áudio 3D real" gap's real ceiling, not a choice made here -- the
    // approximation below (distance -> volume, left/right cross product -> pan) is as close as
    // this library gets. kMaxAudibleDistance is generous relative to the scene's scale (the
    // interact radius alone already puts the player within a few units of the reactor at trigger
    // time), so this mostly reads as "close, therefore loud" -- a real test of falloff would need
    // a scene where the listener can be far from the trigger.
    constexpr float kMaxAudibleDistance = 15.0f;

    // The one real IScreenElement so far: renders every Renderable via app/scene/renderable.h
    // instead of hardcoding geometry per entity (contrast game/sandbox's GameplayScene, which
    // still does). Now also draws the scene's Skybox first, inside the same BeginMode3D/EndMode3D
    // block -- a skybox needs the active camera's projection/view matrices, so it can't be its own
    // independent IScreenElement without either duplicating this block or sharing a Camera3D across
    // two elements' render calls; simplest to keep it here, same reasoning DrawGrid already follows.
    class FlareReactorScene : public IScreenElement {
    public:
        FlareReactorScene(entt::registry &registry, const Camera3D &camera, const Skybox &skybox,
                           const Lighting &lighting)
            : registry_(registry), camera_(camera), skybox_(skybox), lighting_(lighting) {}

        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
            (void)dt;

            BeginMode3D(camera_);
            skybox_.Draw();   // first -- everything else draws on top of it
            lighting_.Update(registry_, camera_.position);
            DrawRenderables(registry_, &lighting_.GetShader());
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
        const Skybox &skybox_;
        const Lighting &lighting_;
        int zOrder_ = 0;
        bool visible_ = true;
    };

    // Translates ACTION_INTERACT into EvtData_ActivateBeacon -- nothing more. Deliberately does no
    // proximity/state validation of its own (see events.h's header comment): the view's job is
    // "what did the player ask for", not "is that legal right now" -- same division
    // TeapotController.cpp has in the book. FlareReactorGameLogic::OnActivateBeacon is where that
    // gets decided. Only pushed once VOnAttach knows which actor this view possesses (see below) --
    // an observer-only view with no possessed actor never gets one.
    class PlayerInteractElement : public IScreenElement {
    public:
        PlayerInteractElement(EventManager &events, InputBindings &input, entt::entity player)
            : events_(events), input_(input), player_(player) {}

        void VOnUpdate(float dt) override {
            (void)dt;
            if (input_.IsPressed(InputAction::Interact)) {
                TraceLog(LOG_INFO, "PlayerInteractElement: Interact pressed -- emitting EvtData_ActivateBeacon");
                events_.Emit(EvtData_ActivateBeacon{player_});
            }
        }

        void VOnRender(float dt) override { (void)dt; }

        int VGetZOrder() const override { return zOrder_; }
        void VSetZOrder(int zOrder) override { zOrder_ = zOrder; }
        bool VIsVisible() const override { return visible_; }
        void VSetVisible(bool visible) override { visible_ = visible; }

    private:
        EventManager &events_;
        InputBindings &input_;
        entt::entity player_;
        int zOrder_ = 0;
        bool visible_ = true;
    };
}

FlareReactorView::FlareReactorView(entt::registry &registry, EventManager &events, ResourceCache<Sound> &sounds,
                                    const std::string &skyboxCubemapPath, const std::string &beaconSoundPath,
                                    Lighting &lighting)
    : registry_(registry), events_(events), sounds_(sounds), input_(LoadOrCreateInputBindings()),
      skybox_(skyboxCubemapPath), lighting_(lighting) {
    camera_.position = Vector3{0.0f, 12.0f, 12.0f};
    camera_.target = Vector3{0.0f, 0.0f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    PushElement(std::make_unique<FlareReactorScene>(registry_, camera_, skybox_, lighting_));

    // Path from GameConfig (game_config.h/assets/config/flare_reactor/game.yaml), not hardcoded --
    // reused from game/sandbox (RFC-0001 Phase 5's own decision -- no dedicated beacon sound asset
    // exists yet; coin.wav is a placeholder, easy to swap for a real one later, just a YAML edit).
    beaconSound_ = sounds_.GetHandle(beaconSoundPath);

    events_.Subscribe<EvtData_BeaconTriggered>(
        [this](const EvtData_BeaconTriggered &event) { OnBeaconTriggered(event); });
}

void FlareReactorView::OnBeaconTriggered(const EvtData_BeaconTriggered &event) {
    TraceLog(LOG_INFO, "FlareReactorView: EvtData_BeaconTriggered received");

    if (!beaconSound_) return;

    // Approximate "3D" audio, computed once at trigger time from camera_ vs. the event's position
    // -- see the kMaxAudibleDistance comment above for why this is the real ceiling, not a partial
    // step toward something better without changing library.
    Vector3 toSound = Vector3Subtract(event.position, camera_.target);
    float distance = Vector3Length(toSound);
    float volume = Clamp(1.0f - distance / kMaxAudibleDistance, 0.0f, 1.0f);

    // Vector3Normalize returns its input unchanged for a zero-length vector (raymath.h), so a
    // trigger at the exact camera target position falls back to centered pan (0.5) rather than NaN.
    Vector3 right = Vector3CrossProduct(Vector3Subtract(camera_.target, camera_.position), camera_.up);
    float pan = Clamp(0.5f + Vector3DotProduct(Vector3Normalize(toSound), Vector3Normalize(right)) * 0.5f,
                       0.0f, 1.0f);

    SetSoundVolume(*beaconSound_, volume);
    SetSoundPan(*beaconSound_, pan);
    PlaySound(*beaconSound_);

    TraceLog(LOG_INFO, "FlareReactorView: playing beacon sound (volume %.2f, pan %.2f)", volume, pan);
}

void FlareReactorView::VOnAttach(GameViewId id, std::optional<entt::entity> actorId) {
    HumanViewBase::VOnAttach(id, actorId);

    if (actorId.has_value()) {
        PushElement(std::make_unique<PlayerInteractElement>(events_, input_, *actorId));
    }
}

void FlareReactorView::VOnUpdate(float dt) {
    UpdateElements(dt);

    // KEY_TAB: dev/validation toggle, not an InputBindings action -- see the header comment on
    // freeCameraActive_. DisableCursor()/EnableCursor() match raylib's own camera examples: locks
    // and hides the OS cursor so CAMERA_FREE's mouse-look reads a continuous delta instead of
    // hitting the window edge.
    if (IsKeyPressed(KEY_TAB)) {
        freeCameraActive_ = !freeCameraActive_;
        if (freeCameraActive_) {
            DisableCursor();
            TraceLog(LOG_INFO, "FlareReactorView: free camera ON (mouse look + WASD/space/ctrl -- TAB to return)");
        } else {
            EnableCursor();
            TraceLog(LOG_INFO, "FlareReactorView: free camera OFF -- gameplay camera resumes following the player");
        }
    }

    // KEY_P: dev/validation tool (screenshot_capture.h), same raw-key category as KEY_TAB above --
    // not an InputBindings action. Queue<T>, not Emit<T>: EventManager::DispatchQueued() runs at
    // the very start of the *next* frame's TickAndUpdateDraw (app/core/engine.cpp), before that
    // frame's own BeginDrawing/EndDrawing -- so ScreenshotCapture's TakeScreenshot() call ends up
    // reading the front buffer exactly as it looked right after *this* frame's EndDrawing (below),
    // i.e. the frame the player actually saw when they pressed P. Emit<T> here instead would run
    // immediately, before this frame has even drawn -- one frame stale.
    if (IsKeyPressed(KEY_P)) {
        TraceLog(LOG_INFO, "FlareReactorView: KEY_P pressed -- queuing EvtData_ScreenshotRequested");
        events_.Queue(EvtData_ScreenshotRequested{});
    }

    if (freeCameraActive_) {
        // raylib's own built-in controller (rcamera.h) -- mouse-look, WASD to move, space/ctrl for
        // up/down, wheel to dolly toward camera_.target. Player position and the gameplay camera's
        // follow logic below are frozen while this is active; toggling back off snaps the gameplay
        // camera back to the player on the very next frame (the block below runs unconditionally
        // once freeCameraActive_ is false again), which is fine for a validation tool.
        UpdateCamera(&camera_, CAMERA_FREE);
        return;
    }

    if (!possessedActor_.has_value()) return;

    LocalTransform *transform = registry_.try_get<LocalTransform>(*possessedActor_);
    if (transform == nullptr) return;

    // ADR-0013's InputManager, not raw IsKeyDown -- input_ is loaded once from
    // config/keybindings.yaml (or written with defaults on first run), rebindable without a
    // recompile. Axis mapping matches ADR-0013 §4's own sketch exactly.
    Vector3 move{0.0f, 0.0f, 0.0f};
    if (input_.IsDown(InputAction::MoveForward))  move.z -= 1.0f;
    if (input_.IsDown(InputAction::MoveBackward)) move.z += 1.0f;
    if (input_.IsDown(InputAction::MoveLeft))     move.x -= 1.0f;
    if (input_.IsDown(InputAction::MoveRight))    move.x += 1.0f;

    transform->position = Vector3Add(transform->position, Vector3Scale(move, kMoveUnitsPerSecond * dt));

    camera_.target = transform->position;
    camera_.position = Vector3Add(transform->position, Vector3{0.0f, 12.0f, 12.0f});
}

// VOnRender/PushElement/RemoveElement live in HumanViewBase (app/view/human_view_base.cpp) now --
// nothing flare_reactor-specific about them.
