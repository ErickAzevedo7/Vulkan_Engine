#version 450
layout(location = 0) out vec3 worldPos;

layout(set=0, binding=0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrices[6];
    vec4 lightPos_farPlane;
    vec3 viewPos;
} global;

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
    gl_Position = global.proj * global.view * vec4(worldPos, 1.0);
}
