// Per-material data, bind group 2. Must match MaterialUniforms in private/ShaderLayouts.h.

layout(set = 2, binding = 0) uniform sampler2D albedoMap;
layout(set = 2, binding = 1) uniform sampler2D normalMap;
layout(set = 2, binding = 2) uniform sampler2D metallicMap;
layout(set = 2, binding = 3) uniform sampler2D roughnessMap;
layout(set = 2, binding = 4) uniform sampler2D occlusionMap;
layout(set = 2, binding = 5) uniform sampler2D emissiveMap;

layout(set = 2, binding = 6) uniform MaterialUniforms {
    vec4 baseColor;
    vec4 emissiveAndOcclusion;
    vec4 metallicRoughness;
} material;
