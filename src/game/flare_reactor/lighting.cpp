#include "lighting.h"

#include "app/scene/light.h"
#include "app/scene/mesh_renderer.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION 330
#else   // PLATFORM_WEB, PLATFORM_ANDROID -- no glsl100 lighting shader shipped yet (same
        // documented gap as skybox.cpp: flare_reactor isn't built for these platforms today)
    #define GLSL_VERSION 100
#endif

namespace {
    Shader LoadLightingShaderInstance() {
        Shader shader = LoadShader(TextFormat("resources/shaders/glsl%i/lighting.vs", GLSL_VERSION),
                                    TextFormat("resources/shaders/glsl%i/lighting.fs", GLSL_VERSION));
        shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
        return shader;
    }
}

Lighting::Lighting() : shader_(LoadLightingShaderInstance()), primitivesMaterial_{shader_, LoadMaterialDefault(), {}, {}} {}

Lighting::~Lighting() { UnloadShader(shader_); }

void Lighting::Update(entt::registry &registry, Vector3 viewPos) const {
    PushFrameUniforms(registry, viewPos, shader_);
    UpdateMeshRendererFrameUniforms(registry, viewPos);
}
