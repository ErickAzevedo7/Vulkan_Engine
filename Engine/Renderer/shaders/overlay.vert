#version 450

// The 2D layer: positions arrive in window pixels, origin at the top left.

layout(push_constant) uniform OverlayConstants {
    // Pixels to clip space: clip = position * scale + offset.
    vec2 scale;
    vec2 offset;
    uint flags;
} overlay;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

void main() {
    fragUV = inUV;
    fragColor = inColor;
    gl_Position = vec4(inPosition * overlay.scale + overlay.offset, 0.0, 1.0);
}
