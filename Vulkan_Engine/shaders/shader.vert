#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 normal;
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrices[6];
    vec4 lightPos_farPlane;
    vec3 viewPos;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragViewPos;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragPosition = vec3(ubo.model * vec4(inPosition, 1.0));
    fragNormal = mat3(ubo.normal) * inNormal;
    fragTexCoord = inTexCoord;
    fragViewPos = ubo.viewPos;
}