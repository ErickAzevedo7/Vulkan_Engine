// A frame's lights and shadows, bind group 0. Lists as long as the frame needs.
// Must match the Gpu structs in private/ShaderLayouts.h.

#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2

struct Light {
    vec4 positionAndRange;
    vec4 directionAndType;
    vec4 colorAndIntensity;
    // x = cos(inner angle), y = cos(outer angle), z = shadow index or -1: into shadows[],
    // where a point light has six in a row, in the order +X, -X, +Y, -Y, +Z, -Z
    vec4 spotAndShadow;
};

struct Shadow {
    mat4 viewProjection;
    // x = world size of a texel one unit in front of the light. Multiplied by clip w, it works
    // for both projections
    vec4 parameters;
    // Where it is in the shadow atlas, in UVs: xy = corner, zw = size
    vec4 atlasRect;
};

// Directional lights first, then the point and spot lights the clusters refer to.
layout(std430, set = 0, binding = 1) readonly buffer LightList {
    Light lights[];
};

// One per cluster: x = first entry in lightIndices, y = how many.
layout(std430, set = 0, binding = 2) readonly buffer ClusterList {
    uvec2 clusters[];
};

layout(std430, set = 0, binding = 3) readonly buffer LightIndexList {
    uint lightIndices[];
};

layout(std430, set = 0, binding = 4) readonly buffer ShadowList {
    Shadow shadows[];
};
