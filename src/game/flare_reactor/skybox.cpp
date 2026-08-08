#include "skybox.h"

#include <rlgl.h>   // rlDisableBackfaceCulling/rlDisableDepthMask and their re-enable counterparts

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION 330
#else   // PLATFORM_WEB, PLATFORM_ANDROID -- no glsl100 skybox shader shipped yet
        // (assets/shaders/ only has glsl330/); flare_reactor isn't built for these platforms today,
        // so this is a known, documented gap rather than a silent one -- LoadShader below would
        // just fail to find the file and fall back to raylib's default shader (a TraceLog warning,
        // not a crash), which wouldn't sample a cubemap correctly.
    #define GLSL_VERSION 100
#endif

Skybox::Skybox(const std::string &cubemapImagePath) {
    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    model_ = LoadModelFromMesh(cube);

    model_.materials[0].shader =
        LoadShader(TextFormat("resources/shaders/glsl%i/skybox.vs", GLSL_VERSION),
                   TextFormat("resources/shaders/glsl%i/skybox.fs", GLSL_VERSION));
    int envMap = MATERIAL_MAP_CUBEMAP;
    int doGamma = 0;
    int vflipped = 0;
    SetShaderValue(model_.materials[0].shader, GetShaderLocation(model_.materials[0].shader, "environmentMap"),
                   &envMap, SHADER_UNIFORM_INT);
    SetShaderValue(model_.materials[0].shader, GetShaderLocation(model_.materials[0].shader, "doGamma"), &doGamma,
                   SHADER_UNIFORM_INT);
    SetShaderValue(model_.materials[0].shader, GetShaderLocation(model_.materials[0].shader, "vflipped"), &vflipped,
                   SHADER_UNIFORM_INT);

    // Image, not Texture2D -- LoadTextureCubemap slices the loaded image into 6 faces on the CPU
    // side per its detected layout, then uploads the result as one cubemap texture. Loaded directly
    // (not through Engine::Textures()' ResourceCache<Texture2D>) since that cache is keyed/typed for
    // whole Texture2D handles, not this intermediate Image step.
    Image image = LoadImage(cubemapImagePath.c_str());
    model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(image, CUBEMAP_LAYOUT_AUTO_DETECT);
    UnloadImage(image);
}

Skybox::~Skybox() {
    // Unloads the mesh, the cubemap texture, and the skybox shader together (UnloadModel ->
    // UnloadMaterial does all three for a non-default material) -- see the header comment for why
    // this shader is intentionally NOT routed through Engine::GetShader()'s ResourceCache instead.
    UnloadModel(model_);
}

void Skybox::Draw() const {
    rlDisableBackfaceCulling();   // We are inside the cube -- back faces point toward the camera
    rlDisableDepthMask();
    DrawModel(model_, Vector3{0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}
