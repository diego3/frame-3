#include "lighting.h"

#include "app/scene/renderable.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION 330
#else   // PLATFORM_WEB, PLATFORM_ANDROID -- no glsl100 lighting shader shipped yet (same
        // documented gap as skybox.cpp: flare_reactor isn't built for these platforms today)
    #define GLSL_VERSION 100
#endif

namespace {
    struct LightDesc {
        int type;
        Vector3 position;
        Vector3 target;
        Color color;
    };

    // Warm "sun" + a cool point light near the reactor's cyan core (assets/entities/flare_reactor/
    // reactor.yaml's model). Fixed/static for now -- nothing in this experiment moves a light yet.
    //
    // Sun position re-derived 2026-08-08 to actually match the baked-in sun disc in the skybox
    // cubemap (assets/cubemaps/Cubemap_Sky_03-512x512.png, GameConfig's default), not just its
    // color mood -- the previous {-6,8,-4} put the light high overhead from -X/-Z, while the
    // cubemap's own sun sits low near the horizon on the +X face (near its +Z edge): opposite
    // horizontal quadrant *and* wrong altitude. docs/learning/rendering.html has the face-layout
    // math this was read off of. Revisit if the shipped cubemap path ever changes -- this is only
    // calibrated against Cubemap_Sky_03 specifically.
    constexpr LightDesc kLights[2] = {
        {/*LIGHT_DIRECTIONAL=*/0, {6.0f, 1.5f, 3.0f}, {0.0f, 0.0f, 0.0f}, {255, 196, 130, 255}},
        {/*LIGHT_POINT=*/1, {0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {110, 210, 255, 255}},
    };

    // Fresnel rim glow tuning (docs/learning/rendering.html, "Fresnel rim glow") -- cyan to match
    // the core point light above, not physically motivated. Fixed/static for now, same as kLights;
    // revisit if this ever needs to react to game state (e.g. flare up on beacon trigger).
    // kRimIntensity bumped 0.6 -> 1.5 (2026-08-08, user request) to read clearly at a glance instead
    // of needing a close, slow orbit to notice -- easy to retune further, just this one constant.
    constexpr Color kRimColor = {110, 210, 255, 255};
    constexpr float kRimPower = 3.0f;
    constexpr float kRimIntensity = 1.5f;

    // ADR-0019's RenderMaterial::extras bag for rim glow -- what used to be the bespoke SetupRim
    // free function below (removed) is now just "apply this named-uniform bag", the same mechanism
    // any future per-material extra (scroll, pulse) will use. rimColor is a normalized (0..1) vec3,
    // matching lighting.fs's `uniform vec3 rimColor` -- NOT a raylib Color/UniformValue's Color
    // variant, which packs 4 channels (vec4) and would misalign a vec3 uniform.
    std::vector<UniformValue> BuildRimExtras() {
        return {
            {"rimColor", Vector3{kRimColor.r / 255.0f, kRimColor.g / 255.0f, kRimColor.b / 255.0f}},
            {"rimPower", kRimPower},
            {"rimIntensity", kRimIntensity},
        };
    }

    // Applies kLights to `shader` -- see lighting.h's header comment on why this reimplements
    // rlights.h's CreateLight/UpdateLightValues uniform-setting instead of calling them (their
    // shared lightsCount counter isn't safe across N independent shader instances). Uniform names
    // must match resources/shaders/glsl330/lighting.fs's `Light` struct exactly.
    void SetupLights(Shader &shader) {
        int ambientLoc = GetShaderLocation(shader, "ambient");
        float ambient[4] = {0.15f, 0.15f, 0.18f, 1.0f};
        SetShaderValue(shader, ambientLoc, ambient, SHADER_UNIFORM_VEC4);

        for (int i = 0; i < 2; ++i) {
            const LightDesc &light = kLights[i];
            int enabled = 1;
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].enabled", i)), &enabled,
                           SHADER_UNIFORM_INT);
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].type", i)), &light.type,
                           SHADER_UNIFORM_INT);
            float position[3] = {light.position.x, light.position.y, light.position.z};
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].position", i)), position,
                           SHADER_UNIFORM_VEC3);
            float target[3] = {light.target.x, light.target.y, light.target.z};
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].target", i)), target,
                           SHADER_UNIFORM_VEC3);
            float color[4] = {light.color.r / 255.0f, light.color.g / 255.0f, light.color.b / 255.0f,
                               light.color.a / 255.0f};
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].color", i)), color,
                           SHADER_UNIFORM_VEC4);
        }
    }

    // No SetupRim here anymore -- rim is now an opt-in RenderMaterial::extras bag (BuildRimExtras
    // above), applied by whoever actually wants it (ApplyToModel, below) via ApplyExtras
    // (app/scene/material.h), not baked unconditionally into every shader instance this compiles.
    // Compiling a primitives instance (Lighting::Lighting()) and NOT calling ApplyExtras on it is
    // exactly the fix for the bug that motivated ADR-0019: before this, every instance -- including
    // the Sentinel's shared Box/Sphere shader -- got rim glow whether it was supposed to or not.
    Shader LoadLightingShaderInstance() {
        Shader shader = LoadShader(TextFormat("resources/shaders/glsl%i/lighting.vs", GLSL_VERSION),
                                    TextFormat("resources/shaders/glsl%i/lighting.fs", GLSL_VERSION));
        shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
        SetupLights(shader);
        return shader;
    }
}

Lighting::Lighting()
    : shader_(LoadLightingShaderInstance()),
      primitivesMaterial_{shader_, LoadMaterialDefault(), {}} {}

Lighting::~Lighting() {
    // Only unloads shader_ (the primitives instance) -- every reactor Model material's own instance
    // (ApplyToModel) is unloaded by that Model's normal UnloadModel path instead, see the header
    // comment on why this is safe (each material owns an independent GL program, none shared).
    UnloadShader(shader_);
}

void Lighting::ApplyToModel(Model &model) const {
    std::vector<UniformValue> rim = BuildRimExtras();
    for (int i = 0; i < model.materialCount; ++i) {
        Shader matShader = LoadLightingShaderInstance();
        ApplyExtras(matShader, rim);   // baked once here -- see this file's LoadLightingShaderInstance comment
        model.materials[i].shader = matShader;
    }
}

void Lighting::Update(entt::registry &registry, Vector3 viewPos) const {
    float pos[3] = {viewPos.x, viewPos.y, viewPos.z};
    SetShaderValue(shader_, shader_.locs[SHADER_LOC_VECTOR_VIEW], pos, SHADER_UNIFORM_VEC3);

    // Every reactor Model material's own shader instance needs the same per-frame refresh -- each
    // is an independent compiled program (ApplyToModel), so none of them share shader_'s uniform
    // state. GetShaderLocation's string lookup here (not cached) is a per-frame cost proportional to
    // material count -- negligible at this experiment's scale (one Model, ~13 materials); revisit if
    // that ever stops being true.
    auto view = registry.view<Renderable>();
    for (auto entity : view) {
        const Renderable &renderable = view.get<Renderable>(entity);
        if (renderable.shape != Renderable::Shape::Model || !renderable.model) continue;

        for (int i = 0; i < renderable.model->materialCount; ++i) {
            Shader &matShader = renderable.model->materials[i].shader;
            SetShaderValue(matShader, GetShaderLocation(matShader, "viewPos"), pos, SHADER_UNIFORM_VEC3);
        }
    }
}
