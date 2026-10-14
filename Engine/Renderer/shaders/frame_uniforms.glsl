// Per-frame data, bind group 0. Must match FrameUniforms in private/ShaderLayouts.h.

#define MAX_LIGHTS 8
#define MAX_SHADOWS 4

#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2

struct Light {
    vec4 positionAndRange;
    vec4 directionAndType;
    vec4 colorAndIntensity;
    // x = cos(inner angle), y = cos(outer angle), z = shadow index or -1
    vec4 spotAndShadow;
};

struct Shadow {
    mat4 viewProjection;
    // x = world size of a texel, scaled by clip w, y = shadow map layer
    vec4 parameters;
};

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    uvec4 lightCount;
    // x = environment intensity
    vec4 environmentParameters;
    Light lights[MAX_LIGHTS];
    Shadow shadows[MAX_SHADOWS];
} frame;
