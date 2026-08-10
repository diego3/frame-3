// RenderMaterial: ADR-0019's "casa" for a shader's per-object uniform values -- the piece that was
// missing when game/flare_reactor/lighting.cpp wanted fresnel rim glow (rimColor/rimPower/
// rimIntensity) on the reactor's Model materials but NOT on the generic solid Box/Sphere
// Renderables (app/scene/renderable.h) the Sentinel uses. Before this, those uniforms were pushed
// by a bespoke free function (lighting.cpp's SetupRim) straight onto whatever Shader it was handed,
// with no per-object identity to say "this one wants rim, that one doesn't" short of a bool/enum
// parameter threading through the shared shader-compile function. RenderMaterial + ApplyExtras
// below is that identity: a named uniform, or a handful of them, that travels with a specific
// material instance instead of a shader-compile-time flag.
//
// Named RenderMaterial, not Material -- raylib's own raylib.h already defines a global `struct
// Material { Shader shader; MaterialMap *maps; float params[4]; } Material;` (a C-style typedef, no
// namespace). A second global `struct Material` here would be a straight redefinition conflict, not
// a shadow/override -- ADR-0019's own code sketch used `::Material raylibMaterial` assuming a
// namespace wrapper; this project's app/scene/ headers are otherwise flat (Renderable,
// BoxRenderable, WorldTransform all sit in the global namespace, no clash since raylib has no type
// by those names), so a distinct name here matches that convention better than introducing the only
// namespaced header in the directory.
#ifndef RENDER_MATERIAL_H
#define RENDER_MATERIAL_H

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <raylib.h>

// One named uniform value beyond raylib's own fixed Material::maps[]/params[4] slots. A closed
// std::variant, not a `void*` + size/type-tag pair -- every value this project has needed so far
// (rim glow's color/power/intensity today; scroll offset and emissive pulse next, per
// docs/learning/rendering.html's "Roadmap de efeitos do reator") is one of these four raylib-native
// shapes. Revisit if a fifth shape (mat4, texture sampler swap, ...) actually shows up.
struct UniformValue {
    std::string name;
    std::variant<float, Vector3, int, Color> value;
};

// A Shader plus every per-object value it needs to draw correctly: raylib's own fixed Material
// (maps[]/params[4] -- diffuse color/texture, the part every raylib draw call already understands)
// plus this project's extension for anything else a custom shader declares by name (extras, applied
// via ApplyExtras below). Mirrors ADR-0019's own
//   Mesh     -> (vertices, indices, UVs)
//   Material -> (Shader, Texturas, Uniforms, Propriedades)
//   Renderer -> (Bind(Material), Bind(Mesh), Draw())
// sketch -- this struct is the middle row; app/scene/renderer.h is the bottom one.
struct RenderMaterial {
    Shader shader{};
    ::Material raylibMaterial{};
    std::vector<UniformValue> extras;
};

// Applies every extras[i] to `shader` by name, one GetShaderLocation + SetShaderValue call each --
// the same "rimColor"/"rimPower"/"rimIntensity" naming game/flare_reactor/lighting.cpp's own
// (now-removed) SetupRim pushed by hand, generalized to any name/value pair. A uniform absent from
// this particular shader (loc == -1, e.g. a primitives shader with no rim extras applied) is
// silently skipped, not an error -- same as raylib's own SetShaderValue already tolerates a -1 loc.
//
// GetShaderLocation's string lookup is not cached here -- same tradeoff game/flare_reactor/
// lighting.cpp's Update() already accepted for its per-frame viewPos push ("negligible at this
// project's scale"); revisit together if that ever stops being true.
inline void ApplyExtras(const Shader &shader, const std::vector<UniformValue> &extras) {
    for (const UniformValue &extra : extras) {
        int loc = GetShaderLocation(shader, extra.name.c_str());
        if (loc == -1) continue;

        std::visit(
            [&](auto &&v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, float>) {
                    SetShaderValue(shader, loc, &v, SHADER_UNIFORM_FLOAT);
                } else if constexpr (std::is_same_v<T, int>) {
                    SetShaderValue(shader, loc, &v, SHADER_UNIFORM_INT);
                } else if constexpr (std::is_same_v<T, Vector3>) {
                    float packed[3] = {v.x, v.y, v.z};
                    SetShaderValue(shader, loc, packed, SHADER_UNIFORM_VEC3);
                } else if constexpr (std::is_same_v<T, Color>) {
                    float packed[4] = {v.r / 255.0f, v.g / 255.0f, v.b / 255.0f, v.a / 255.0f};
                    SetShaderValue(shader, loc, packed, SHADER_UNIFORM_VEC4);
                }
            },
            extra.value);
    }
}

#endif // RENDER_MATERIAL_H
