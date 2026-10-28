#version 450

#extension GL_GOOGLE_include_directive : require
#include "color_space.glsl"

// Maps the scene's high dynamic range color to what a display can show.

layout(set = 0, binding = 0) uniform sampler2D sceneColor;

layout(push_constant) uniform TonemapConstants {
    float exposure;
    // 1 for outputs that store sRGB-encoded values as they are (UNORM formats); 0 for
    // _SRGB formats, which encode the linear result themselves.
    uint encodeSrgb;
} tonemap;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

// Filmic curve fitted to the ACES tonemapper by Krzysztof Narkowicz.
vec3 TonemapACES(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main() {
    vec3 color = TonemapACES(texture(sceneColor, fragUV).rgb * tonemap.exposure);
    if (tonemap.encodeSrgb != 0) {
        color = LinearToSrgb(color);
    }
    outColor = vec4(color, 1.0);
}
