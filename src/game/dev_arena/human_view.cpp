#include "human_view.h"

#include <memory>
#include <raymath.h>

#include "app/entity/entity_file_parser_yaml.h"
#include "app/io/file_io.h"
#include "app/scene/light.h"
#include "app/scene/material_loader.h"
#include "app/scene/renderable.h"
#include "app/scene/renderer.h"
#include "app/scene/transform.h"
#include "app/view/debug_overlay_screen_element.h"
#include "app/view/screen_element.h"

namespace {
    constexpr float kMoveUnitsPerSecond = 4.0f;

    // Loads assets/materials/dev_arena.mat.yaml (docs/adr/0020's ".mat" format, unchanged) through
    // the same LoadRenderMaterial every MeshRenderer consumer already uses -- nothing dev_arena-
    // specific here, this is the intended general-purpose entry point for "a RenderMaterial
    // described in YAML", not something previously scoped to MeshRenderer alone.
    RenderMaterial LoadDevArenaMaterial(ResourceCache<Texture2D> &textures) {
        YamlEntityFileParser parser;
        return LoadRenderMaterial("resources/materials/dev_arena.mat.yaml", parser, ReadWholeFile, textures);
    }

    // The one real IScreenElement here (docs/adr/0016): draws every Renderable (app/scene/
    // renderable.h) through material_ (checkerboard-textured, tinted per entity via
    // Renderable::color) plus a procedural DrawGrid for scale/orientation reference -- no
    // Renderable-based floor (a heavily stretched unit-cube's UVs don't retile, see
    // assets/materials/dev_arena.mat.yaml's own header comment on why individual near-unit-scale
    // boxes are used instead of a textured floor plane). Pushes light.h's PushFrameUniforms every
    // frame first, same "Update() before Draw()" split game/flare_reactor/lighting.cpp/
    // mesh_renderer.h's UpdateMeshRendererFrameUniforms already establish -- collects any
    // WorldTransform+Light entity (assets/entities/dev_arena/sun.yaml) into material_'s shader.
    class DevArenaScene : public IScreenElement {
    public:
        DevArenaScene(entt::registry &registry, const Camera3D &camera, RenderMaterial &material)
            : registry_(registry), camera_(camera), material_(material) {}

        void VOnUpdate(float dt) override { (void)dt; }

        void VOnRender(float dt) override {
            (void)dt;
            PushFrameUniforms(registry_, camera_.position, material_.shader);

            BeginMode3D(camera_);
            DrawRenderables(registry_, &material_);
            DrawGrid(40, 1.0f);
            EndMode3D();
        }

        int VGetZOrder() const override { return zOrder_; }
        void VSetZOrder(int zOrder) override { zOrder_ = zOrder; }
        bool VIsVisible() const override { return visible_; }
        void VSetVisible(bool visible) override { visible_ = visible; }

    private:
        entt::registry &registry_;
        const Camera3D &camera_;
        RenderMaterial &material_;
        int zOrder_ = 0;
        bool visible_ = true;
    };
}

DevArenaView::DevArenaView(entt::registry &registry, ResourceCache<Texture2D> &textures)
    : registry_(registry), input_(LoadOrCreateInputBindings()), material_(LoadDevArenaMaterial(textures)) {
    // Pulled back enough to see the whole six-cube reference row (assets/levels/dev_arena.yaml
    // spans x=-5..5) plus the player, not just game/sandbox's tighter default framing.
    camera_.position = Vector3{0.0f, 12.0f, 16.0f};
    camera_.target = Vector3{0.0f, 0.0f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    PushElement(std::make_unique<DevArenaScene>(registry_, camera_, material_));
    // Single-scene main() (no screens.h state machine) -- alive for the whole run, same as
    // game/camera_fps's own DebugOverlayScreenElement use, unlike game/sandbox's HumanView (only
    // alive during GAMEPLAY, per that element's own header comment on why it's sandbox-excluded).
    PushElement(std::make_unique<DebugOverlayScreenElement>());
}

DevArenaView::~DevArenaView() { UnloadShader(material_.shader); }

void DevArenaView::VOnUpdate(float dt) {
    UpdateElements(dt);

    if (!possessedActor_.has_value()) return;

    LocalTransform *transform = registry_.try_get<LocalTransform>(*possessedActor_);
    if (transform == nullptr) return;

    Vector3 move{0.0f, 0.0f, 0.0f};
    if (input_.IsDown(InputAction::MoveForward))  move.z -= 1.0f;
    if (input_.IsDown(InputAction::MoveBackward)) move.z += 1.0f;
    if (input_.IsDown(InputAction::MoveLeft))     move.x -= 1.0f;
    if (input_.IsDown(InputAction::MoveRight))    move.x += 1.0f;

    transform->position = Vector3Add(transform->position, Vector3Scale(move, kMoveUnitsPerSecond * dt));

    // Camera trails the possessed actor, same as game/sandbox/human_view.cpp's own movement --
    // walking past the reference-cube row is the whole point of a scale-reference scene.
    camera_.target = transform->position;
    camera_.position = Vector3Add(transform->position, Vector3{0.0f, 12.0f, 16.0f});
}
