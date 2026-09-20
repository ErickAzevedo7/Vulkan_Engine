// Per-frame data, bind group 0. Must match FrameUniforms in private/ShaderLayouts.h.

#define MAX_LIGHTS 8

#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1

struct Light {
    vec4 positionAndRange;
    vec4 directionAndType;
    vec4 colorAndIntensity;
};

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    uvec4 lightCount;
    Light lights[MAX_LIGHTS];
} frame;
