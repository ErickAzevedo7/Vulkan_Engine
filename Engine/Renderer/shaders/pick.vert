#version 450

// Draws each mesh with the ID of its instance, for picking with the mouse.

// Must match PickConstants in private/PickPass.cpp.
layout(push_constant) uniform PickConstants {
    mat4 modelViewProjection;
    // The instance's index + 1, so 0 means nothing, and its generation.
    uvec2 id;
} pick;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = pick.modelViewProjection * vec4(inPosition, 1.0);
}
