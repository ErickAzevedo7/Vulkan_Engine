// Environment and shadows, bind group 1.

layout(set = 1, binding = 0) uniform samplerCube irradianceMap;
layout(set = 1, binding = 1) uniform samplerCube prefilterMap;
layout(set = 1, binding = 2) uniform sampler2D brdfLut;
layout(set = 1, binding = 3) uniform samplerCube environmentMap;
// One layer per shadow, compared against a reference depth when sampled.
layout(set = 1, binding = 4) uniform sampler2DArrayShadow shadowMaps;
// One cube per point light shadow, holding depth as seen from the light.
layout(set = 1, binding = 5) uniform samplerCubeArrayShadow pointShadowMaps;

// Mip count of the prefiltered map, minus one.
const float kMaxReflectionLod = 4.0;
