#include "human_view.h"

#include <raymath.h>
#include <vector>

#include "app/scene/renderable.h"
#include "app/scene/renderer.h"
#include "app/scene/transform.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION 330
#else   // PLATFORM_WEB, PLATFORM_ANDROID -- no glsl100 lighting shader shipped yet, same
        // documented gap game/flare_reactor/lighting.cpp/skybox.cpp already carry
    #define GLSL_VERSION 100
#endif

namespace {
    // Placeholder input scheme -- raw key/gesture to actor-action mapping is an open question
    // (ADR-0010's own Open Questions, still not designed here); this hardcodes arrow keys to X/Z
    // movement so there's something to observe, not a real bindings layer.
    constexpr float kMoveUnitsPerSecond = 4.0f;

    // ADR-0019's second Material/Renderer consumer, built independently of game/flare_reactor's own
    // (see human_view.h's member comment): one directional light (just enough for the lit shader to
    // read as more than flat black -- game/flare_reactor/lighting.cpp's own kLights has the same
    // "warm sun" shape, tuned differently here on purpose, not copied) plus the same rim-glow extras
    // technique, amber instead of the reactor's cyan -- proving the *API* generalizes to an
    // independently-built consumer, not that the visual has to match.
    constexpr Color kRimColor = {255, 170, 60, 255};
    constexpr float kRimPower = 3.0f;
    constexpr float kRimIntensity = 1.2f;

    void SetupSandboxLighting(Shader &shader) {
        int ambientLoc = GetShaderLocation(shader, "ambient");
        float ambient[4] = {0.2f, 0.2f, 0.22f, 1.0f};
        SetShaderValue(shader, ambientLoc, ambient, SHADER_UNIFORM_VEC4);

        int enabled = 1;
        int type = 0;   // LIGHT_DIRECTIONAL
        Vector3 lightPos{4.0f, 6.0f, 4.0f};
        Vector3 lightTarget{0.0f, 0.0f, 0.0f};
        Color lightColor = WHITE;
        SetShaderValue(shader, GetShaderLocation(shader, "lights[0].enabled"), &enabled, SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, "lights[0].type"), &type, SHADER_UNIFORM_INT);
        float pos[3] = {lightPos.x, lightPos.y, lightPos.z};
        SetShaderValue(shader, GetShaderLocation(shader, "lights[0].position"), pos, SHADER_UNIFORM_VEC3);
        float target[3] = {lightTarget.x, lightTarget.y, lightTarget.z};
        SetShaderValue(shader, GetShaderLocation(shader, "lights[0].target"), target, SHADER_UNIFORM_VEC3);
        float color[4] = {lightColor.r / 255.0f, lightColor.g / 255.0f, lightColor.b / 255.0f,
                           lightColor.a / 255.0f};
        SetShaderValue(shader, GetShaderLocation(shader, "lights[0].color"), color, SHADER_UNIFORM_VEC4);
    }

    // Loads the shared lighting.vs/.fs shader (same GLSL source game/flare_reactor/lighting.cpp
    // compiles its own independent instances from -- reused as a shader resource, not as a class;
    // see human_view.h's member comment), sets up its ambient/light uniforms, and wraps it in a
    // RenderMaterial carrying the rim glow extras above. Caller (HumanView's constructor) owns the
    // result and is responsible for UnloadShader(material.shader) before it goes away -- see
    // ~HumanView.
    RenderMaterial LoadSandboxMaterial() {
        Shader shader = LoadShader(TextFormat("resources/shaders/glsl%i/lighting.vs", GLSL_VERSION),
                                    TextFormat("resources/shaders/glsl%i/lighting.fs", GLSL_VERSION));
        shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
        SetupSandboxLighting(shader);

        std::vector<UniformValue> extras = {
            {"rimColor", Vector3{kRimColor.r / 255.0f, kRimColor.g / 255.0f, kRimColor.b / 255.0f}},
            {"rimPower", kRimPower},
            {"rimIntensity", kRimIntensity},
        };
        return RenderMaterial{shader, LoadMaterialDefault(), std::move(extras)};
    }

    // First real IScreenElement (docs/adr/0016): wraps what used to be HumanView::VOnRender's
    // direct body -- same rendering, now reachable/orderable as an element instead of hardcoded.
    // Holds camera_/material_ by reference to HumanView's own members (updated each frame by
    // HumanView::VOnUpdate) -- safe for GameplayScene's whole lifetime, since it's owned by the
    // same HumanView instance whose members they point at.
    //
    // Draws every entity's Renderable via app/scene/renderable.h's DrawRenderables now, instead of
    // hardcoding a MAROON DrawCubeWires per WorldTransform -- closes the gap renderable.h's own
    // header comment and ADR-0018 both flagged ("game/sandbox's GameplayScene ... still hardcoded a
    // 1x1x1 MAROON DrawCubeWires"), and ADR-0019's Plan item 4.
    class GameplayScene : public IScreenElement {
    public:
        GameplayScene(entt::registry &registry, const Camera3D &camera, const RenderMaterial &material)
            : registry_(registry), camera_(camera), material_(material) {}

        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
            (void)dt;

            BeginMode3D(camera_);
            DrawRenderables(registry_, &material_);
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
        const RenderMaterial &material_;
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
    : registry_(registry), processes_(processes), sounds_(sounds), material_(LoadSandboxMaterial()) {
    camera_.position = Vector3{0.0f, 10.0f, 10.0f};
    camera_.target = Vector3{0.0f, 0.0f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    PushElement(std::make_unique<GameplayScene>(registry_, camera_, material_));
    PushElement(std::make_unique<GameplayHud>());
}

HumanView::~HumanView() { UnloadShader(material_.shader); }

void HumanView::VOnUpdate(float dt) {
    UpdateElements(dt);

    // Pushes the current camera position to material_'s shader (needed for lighting.fs's specular
    // and rim terms) -- same per-frame requirement game/flare_reactor/lighting.cpp's own Update()
    // documents, just inline here since sandbox has no Lighting-class equivalent of its own.
    float viewPos[3] = {camera_.position.x, camera_.position.y, camera_.position.z};
    SetShaderValue(material_.shader, material_.shader.locs[SHADER_LOC_VECTOR_VIEW], viewPos, SHADER_UNIFORM_VEC3);

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
