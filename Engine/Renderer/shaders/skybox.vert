#version 450
#extension GL_GOOGLE_include_directive : require

#include "cube_positions.glsl"
#include "frame_uniforms.glsl"

layout(location = 0) out vec3 fragDirection;

void main() {
    vec3 position = kCubePositions[gl_VertexIndex];
    fragDirection = position;

    // Only the camera's rotation matters, and the w divide pins depth at the far plane.
    mat4 rotationOnly = mat4(mat3(frame.view));
    vec4 clipPosition = frame.projection * rotationOnly * vec4(position, 1.0);
    gl_Position = clipPosition.xyww;
}
