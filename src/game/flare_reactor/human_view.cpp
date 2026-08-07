#include "human_view.h"

#include <algorithm>

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
    // still does).
    class FlareReactorScene : public IScreenElement {
    public:
        FlareReactorScene(entt::registry &registry, const Camera3D &camera)
            : registry_(registry), camera_(camera) {}

        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
            (void)dt;

            BeginMode3D(camera_);
            DrawRenderables(registry_);
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

FlareReactorView::FlareReactorView(entt::registry &registry, EventManager &events, ResourceCache<Sound> &sounds)
    : registry_(registry), events_(events), sounds_(sounds), input_(LoadOrCreateInputBindings()) {
    camera_.position = Vector3{0.0f, 12.0f, 12.0f};
    camera_.target = Vector3{0.0f, 0.0f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    PushElement(std::make_unique<FlareReactorScene>(registry_, camera_));

    // Reused from game/sandbox (RFC-0001 Phase 5's own decision -- no dedicated beacon sound asset
    // exists yet; coin.wav is a placeholder, easy to swap for a real one later without touching
    // any code, just the path here).
    beaconSound_ = sounds_.GetHandle("resources/audio/fx/coin.wav");

    events_.Subscribe<EvtData_BeaconTriggered>(
        [this](const EvtData_BeaconTriggered &event) { OnBeaconTriggered(event); });
}

void FlareReactorView::OnBeaconTriggered(const EvtData_BeaconTriggered &event) {
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
}

void FlareReactorView::VOnAttach(GameViewId id, std::optional<entt::entity> actorId) {
    id_ = id;
    possessedActor_ = actorId;

    if (actorId.has_value()) {
        PushElement(std::make_unique<PlayerInteractElement>(events_, input_, *actorId));
    }
}

void FlareReactorView::VOnUpdate(float dt) {
    for (auto &[id, element] : elements_) element->VOnUpdate(dt);

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

void FlareReactorView::VOnRender(float dt) {
    std::stable_sort(elements_.begin(), elements_.end(), [](const auto &a, const auto &b) {
        return a.second->VGetZOrder() < b.second->VGetZOrder();
    });

    for (auto &[id, element] : elements_) {
        if (element->VIsVisible()) element->VOnRender(dt);
    }
}

ScreenElementId FlareReactorView::PushElement(std::unique_ptr<IScreenElement> element) {
    ScreenElementId id = nextElementId_++;
    elements_.emplace_back(id, std::move(element));
    return id;
}

void FlareReactorView::RemoveElement(ScreenElementId id) {
    std::erase_if(elements_, [id](const auto &pair) { return pair.first == id; });
}
