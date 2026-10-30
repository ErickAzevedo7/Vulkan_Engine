#version 450

#extension GL_GOOGLE_include_directive : require
#include "color_space.glsl"

// Colors arrive sRGB-encoded, like CSS colors, and are blended that way, as the view texture
// stores them.

// Must match the flag values in private/WorldPass.cpp.
const uint kLines = 1u;
const uint kTextureIsLinear = 2u;

layout(set = 0, binding = 0) uniform sampler2D worldTexture;

layout(push_constant) uniform WorldConstants {
    mat4 viewProjection;
    vec2 viewportSize;
    vec2 fade;
    vec3 cameraPosition;
    uint flags;
} world;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragWorldPosition;
layout(location = 3) in float fragEdge;
layout(location = 4) in float fragHalfWidth;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 color = fragColor;
    if ((world.flags & kLines) != 0u) {
        // Full inside the width, fading over the last pixel.
        color.a *= clamp(fragHalfWidth + 0.5 - abs(fragEdge), 0.0, 1.0);
    } else {
        vec4 sampled = texture(worldTexture, fragUV);
        if ((world.flags & kTextureIsLinear) != 0u) {
            sampled.rgb = LinearToSrgb(sampled.rgb);
        }
        color *= sampled;
    }

    if (world.fade.y > 0.0) {
        float distanceToCamera = distance(fragWorldPosition, world.cameraPosition);
        color.a *= 1.0 - smoothstep(world.fade.x, world.fade.y, distanceToCamera);
    }
    if (color.a <= 0.0) discard;
    outColor = color;
}
