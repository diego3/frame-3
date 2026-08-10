#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add your custom variables here

#define     MAX_LIGHTS              4
#define     LIGHT_DIRECTIONAL       0
#define     LIGHT_POINT             1

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

// Input lighting values
uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;

// Fresnel rim glow -- view-dependent edge highlight (docs/learning/rendering.html, "Fresnel rim
// glow"). Not physically-based reflectance, just the pow(1-N.V, power) cheat: additive so it reads
// as light leaving the edge instead of tinting/darkening it.
uniform vec3 rimColor;
uniform float rimPower;
uniform float rimIntensity;

// Scrolling core energy texture (docs/learning/rendering.html, "Scrolling core texture (energy
// flow)") -- a noise/energy texture sliding along U over time, read additively same as rim glow
// above (a surface "emitting" light shouldn't be darkened by ambient/shadow it's not receiving).
// `time` is pushed every frame (Lighting::Update, like viewPos); scrollSpeed/energyIntensity are
// static, set once when this shader instance's RenderMaterial is built (Lighting::ApplyToModel).
//
// texture1, not a custom-named sampler: raylib's DrawMesh (rmodels.c) only auto-binds textures it
// finds in Material::maps[] on every draw, using the fixed default names LoadShader resolves for
// slots 0-2 (texture0 = diffuse/albedo, texture1 = specular/metalness, texture2 = normal) -- a
// uniform sampler2D under any other name has no such per-draw rebinding, so it silently shows
// nothing (SetShaderValueTexture's texture-unit registration is only ever consumed by raylib's
// immediate-mode batch renderer, which DrawMesh doesn't go through). Nothing in this shader reads
// specular/metalness texture data today (the specular term below is a fixed `shine` constant), so
// slot 1 is free to repurpose for the scrolling energy texture instead -- Lighting::ApplyToModel
// sets model.materials[i].maps[MATERIAL_MAP_SPECULAR].texture directly, no custom uniform push.
uniform sampler2D texture1;
uniform float time;
uniform float scrollSpeed;
uniform float energyIntensity;
uniform vec3 energyColor;

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 lightDot = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);

    vec4 tint = colDiffuse*fragColor;

    // NOTE: Implement here your fragment shader code

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 1)
        {
            vec3 light = vec3(0.0);

            if (lights[i].type == LIGHT_DIRECTIONAL)
            {
                light = -normalize(lights[i].target - lights[i].position);
            }

            if (lights[i].type == LIGHT_POINT)
            {
                light = normalize(lights[i].position - fragPosition);
            }

            float NdotL = max(dot(normal, light), 0.0);
            lightDot += lights[i].color.rgb*NdotL;

            float specCo = 0.0;
            if (NdotL > 0.0) specCo = pow(max(0.0, dot(viewD, reflect(-(light), normal))), 16.0); // 16 refers to shine
            specular += specCo;
        }
    }

    finalColor = (texelColor*((tint + vec4(specular, 1.0))*vec4(lightDot, 1.0)));
    finalColor += texelColor*(ambient/10.0)*tint;

    // Fresnel rim glow: strongest where the surface normal grazes the view direction (edges,
    // silhouettes), ~0 where it faces the camera head-on. Additive, unlit by texelColor/tint on
    // purpose -- it's meant to read as the surface emitting light at its edge, not reflecting it.
    float fresnel = pow(1.0 - max(dot(normal, viewD), 0.0), rimPower);
    finalColor.rgb += fresnel*rimColor*rimIntensity;

    // Scrolling core energy texture: additive, same reasoning as rim glow above. energyIntensity
    // defaults to 0 for any material that never calls ApplyExtras with it (e.g. every solid
    // Box/Sphere Renderable drawn with Lighting::GetPrimitivesMaterial()) -- OpenGL zero-initializes
    // a default-block uniform nobody ever calls SetShaderValue on (spec section 2.11.4), so this
    // term is a no-op there regardless of what texture1/scrolledUV sample, same guarantee
    // rimIntensity already relies on above.
    //
    // texture1 (energy-noise.png) is grayscale Perlin noise -- sampling it raw and adding it just
    // brightens the surface in a moving mottled pattern, reads as "static", not "energy". energyColor
    // tints it (same idea as rimColor tinting the Fresnel term) so it actually reads as a colored
    // glow, not a luminance ripple.
    vec2 scrolledUV = fragTexCoord + vec2(time*scrollSpeed, 0.0);
    float energyNoise = texture(texture1, scrolledUV).r;
    finalColor.rgb += energyNoise*energyColor*energyIntensity;

    // Gamma correction
    finalColor = pow(finalColor, vec4(1.0/2.2));
}
