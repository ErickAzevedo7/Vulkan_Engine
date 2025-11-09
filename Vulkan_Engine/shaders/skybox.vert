#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragTexCoord;

layout(set = 0, binding = 0) uniform SkyboxUBO {
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    vec4 pos = ubo.proj * ubo.view * vec4(inPosition, 1.0);
    gl_Position = pos.xyww;
    fragTexCoord = inPosition;
}
