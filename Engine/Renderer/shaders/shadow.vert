#version 450

// Must match ShadowPushConstants in private/ShaderLayouts.h.
layout(push_constant) uniform ShadowPushConstants {
    mat4 modelViewProjection;
} object;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = object.modelViewProjection * vec4(inPosition, 1.0);
}
