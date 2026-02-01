#version 450
layout(location = 0) out vec3 worldPos;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 normal;
    mat4 view;
    mat4 proj;
} ubo;

// Fullscreen quad positions (NDC)
vec2 positions[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

void main() {
    vec2 pos = positions[gl_VertexIndex];
    // Project a large quad in world space (XZ plane, Y=0)
    worldPos = vec3(pos.x * 1000.0, 0.0, pos.y * 1000.0);
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(worldPos, 1.0);
}
