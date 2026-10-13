#version 450
#extension GL_GOOGLE_include_directive : require

#include "cube_positions.glsl"

// Draws one face of a cubemap. The position doubles as the sample direction.
layout(push_constant) uniform CaptureConstants {
    mat4 viewProjection;
    vec4 parameters;
} capture;

layout(location = 0) out vec3 fragDirection;

void main() {
    fragDirection = kCubePositions[gl_VertexIndex];
    gl_Position = capture.viewProjection * vec4(fragDirection, 1.0);
}
