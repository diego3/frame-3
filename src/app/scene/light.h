// Light: ADR-0020's Fase A generalization of game/flare_reactor/lighting.cpp's old private
// kLights[2] array into a real ECS component. Any entity with a WorldTransform + Light is
// collected by PushFrameUniforms below and pushed into whichever Shader's `lights[]` uniform array
// it's asked to update -- not just the reactor's own shader instance, any shader that declares the
// same names (resources/shaders/glsl330/lighting.fs's `Light` struct: enabled/type/position/
// target/color). Header-only, same shape as transform.h/hierarchy.h/renderable.h.
#ifndef LIGHT_H
#define LIGHT_H

#include <string>

#include <entt/entt.hpp>
#include <raylib.h>
#include <raymath.h>

#include "app/entity/entity_def.h"
#include "app/scene/transform.h"

struct Light {
    enum class Type { Directional, Point };

    Type type = Type::Point;
    Color color = WHITE;
};

// Parses type/color from a "Light" component's own EntityDefNode -- the "Light" component loader's
// own body (game/flare_reactor/main.cpp), factored out so RegisterComponentLoaders there stays a
// short list of registrations. Pure (EntityDefNode/Color only, no linked raylib.a symbols) --
// unit-tested directly in tests/light_test.cpp, same "headers OK, .a not" line hierarchy_test.cpp
// already draws.
inline Light ParseLightComponent(const EntityDefNode &node) {
    Light light;
    std::string typeName = "point";
    if (const EntityDefNode *type = node.TryGet("type")) typeName = type->AsString("point");
    light.type = (typeName == "directional") ? Light::Type::Directional : Light::Type::Point;

    if (const EntityDefNode *color = node.TryGet("color")) {
        light.color = Color{static_cast<unsigned char>(color->Get("r").AsInt(255)),
                             static_cast<unsigned char>(color->Get("g").AsInt(255)),
                             static_cast<unsigned char>(color->Get("b").AsInt(255)), 255};
    }
    return light;
}

// lighting.fs's own `#define MAX_LIGHTS 4` -- entries past this index are silently dropped (same
// "no error, just skipped" tolerance ApplyExtras/GetShaderLocation already use elsewhere).
inline constexpr int kMaxLights = 4;

// Pushes ambient (fixed -- same value game/flare_reactor/lighting.cpp's SetupLights always used;
// no per-scene override exists yet, revisit if one's ever needed) + viewPos + time, then one
// lights[i] entry per registry.view<WorldTransform, Light>() entity, to `shader`. Replaces the old
// SetupLights (baked once, at shader-compile time, into a single hardcoded shader instance) with a
// per-frame push any shader instance can receive -- necessary now that lights are entities, not a
// compile-time constant, and RenderMaterial (ADR-0019)/MeshRenderer (ADR-0020) both compile their
// own independent Shader instances (see mesh_renderer.h's own header comment on why sharing one
// instance is safe but deliberately not done yet).
//
// Directional lights' `target` stays Vector3Zero() -- matches the old kLights[0] exactly ("points
// at the origin"); no entity-authored aim direction exists yet (Light's own model is deliberately
// minimal for Fase A, see docs/adr/0020's "Continuar depois").
//
// GetShaderLocation's string lookup isn't cached here, same accepted tradeoff
// game/flare_reactor/lighting.cpp's own (now-generalized) per-frame push always had.
inline void PushFrameUniforms(entt::registry &registry, Vector3 viewPos, const Shader &shader) {
    float ambient[4] = {0.15f, 0.15f, 0.18f, 1.0f};
    SetShaderValue(shader, GetShaderLocation(shader, "ambient"), ambient, SHADER_UNIFORM_VEC4);

    float pos[3] = {viewPos.x, viewPos.y, viewPos.z};
    SetShaderValue(shader, GetShaderLocation(shader, "viewPos"), pos, SHADER_UNIFORM_VEC3);
    float time = (float)GetTime();
    SetShaderValue(shader, GetShaderLocation(shader, "time"), &time, SHADER_UNIFORM_FLOAT);

    int i = 0;
    auto view = registry.view<WorldTransform, Light>();
    for (auto entity : view) {
        if (i >= kMaxLights) break;

        const WorldTransform &world = view.get<WorldTransform>(entity);
        const Light &light = view.get<Light>(entity);
        Vector3 position = Vector3Transform(Vector3Zero(), world.matrix);

        int enabled = 1;
        int type = light.type == Light::Type::Directional ? 0 : 1;   // matches lighting.fs's LIGHT_DIRECTIONAL/LIGHT_POINT
        SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].enabled", i)), &enabled,
                       SHADER_UNIFORM_INT);
        SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].type", i)), &type,
                       SHADER_UNIFORM_INT);
        float posArr[3] = {position.x, position.y, position.z};
        SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].position", i)), posArr,
                       SHADER_UNIFORM_VEC3);
        float target[3] = {0.0f, 0.0f, 0.0f};
        SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].target", i)), target,
                       SHADER_UNIFORM_VEC3);
        float colorArr[4] = {light.color.r / 255.0f, light.color.g / 255.0f, light.color.b / 255.0f,
                              light.color.a / 255.0f};
        SetShaderValue(shader, GetShaderLocation(shader, TextFormat("lights[%d].color", i)), colorArr,
                       SHADER_UNIFORM_VEC4);
        ++i;
    }
}

#endif // LIGHT_H
