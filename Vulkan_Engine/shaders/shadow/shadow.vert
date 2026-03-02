#version 450
#extension GL_EXT_multiview : enable

layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

layout(binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix[6];
    vec4 lightPos_farPlane;
} shadow;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outWorldPos;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    outWorldPos   = worldPos;
    gl_Position   = shadow.lightSpaceMatrix[gl_ViewIndex] * worldPos;
}
