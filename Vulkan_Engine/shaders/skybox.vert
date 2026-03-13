#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragTexCoord;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    mat4 viewRotation = mat4(mat3(ubo.view));
    vec4 pos = ubo.proj * viewRotation * vec4(inPosition, 1.0);
    gl_Position = pos.xyww;
    fragTexCoord = inPosition;
}
