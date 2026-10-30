// Per-frame data, bind group 0. Must match FrameUniforms in private/ShaderLayouts.h.

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    // x = directional lights, the first ones in lights[]
    uvec4 lightCounts;
    // x, y, z = clusters across the screen, down it and in depth
    uvec4 clusterCounts;
    // x, y = clusters per pixel, z, w = scale and bias of a slice: log(depth) * z + w
    vec4 clusterScale;
    // x = environment intensity
    vec4 environmentParameters;
} frame;
