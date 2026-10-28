#version 450

#extension GL_GOOGLE_include_directive : require
#include "color_space.glsl"

// Colors arrive sRGB-encoded, like CSS colors, and are blended that way too.

// Must match the flag values in private/OverlayPass.cpp.
const uint kTextureIsLinear = 1u;
const uint kOutputEncodes = 2u;

layout(set = 0, binding = 0) uniform sampler2D overlayTexture;

layout(push_constant) uniform OverlayConstants {
    vec2 scale;
    vec2 offset;
    // kTextureIsLinear: the texture samples as linear color (_SRGB or float formats), so it
    // is encoded before mixing. kOutputEncodes: the attachment is an _SRGB format, which
    // encodes on write, so the result is decoded first.
    uint flags;
} overlay;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 sampled = texture(overlayTexture, fragUV);
    if ((overlay.flags & kTextureIsLinear) != 0u) {
        sampled.rgb = LinearToSrgb(sampled.rgb);
    }
    vec4 color = fragColor * sampled;
    if ((overlay.flags & kOutputEncodes) != 0u) {
        color.rgb = SrgbToLinear(color.rgb);
    }
    outColor = color;
}
