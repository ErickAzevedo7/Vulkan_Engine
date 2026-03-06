#version 450

// Push constant: one of the 6 capture view matrices
layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
} pc;

// Local position passed to fragment shader as the sample direction
layout(location = 0) out vec3 localPos;

// Unit cube positions – we hard-code them and index by gl_VertexIndex
// to avoid needing a vertex buffer.  36 vertices, 12 triangles.
const vec3 positions[36] = vec3[](
    // +X face
    vec3( 1,-1,-1), vec3( 1,-1, 1), vec3( 1, 1, 1),
    vec3( 1, 1, 1), vec3( 1, 1,-1), vec3( 1,-1,-1),
    // -X face
    vec3(-1,-1, 1), vec3(-1,-1,-1), vec3(-1, 1,-1),
    vec3(-1, 1,-1), vec3(-1, 1, 1), vec3(-1,-1, 1),
    // +Y face
    vec3(-1, 1,-1), vec3( 1, 1,-1), vec3( 1, 1, 1),
    vec3( 1, 1, 1), vec3(-1, 1, 1), vec3(-1, 1,-1),
    // -Y face
    vec3(-1,-1, 1), vec3( 1,-1, 1), vec3( 1,-1,-1),
    vec3( 1,-1,-1), vec3(-1,-1,-1), vec3(-1,-1, 1),
    // +Z face
    vec3(-1,-1, 1), vec3(-1, 1, 1), vec3( 1, 1, 1),
    vec3( 1, 1, 1), vec3( 1,-1, 1), vec3(-1,-1, 1),
    // -Z face
    vec3( 1,-1,-1), vec3( 1, 1,-1), vec3(-1, 1,-1),
    vec3(-1, 1,-1), vec3(-1,-1,-1), vec3( 1,-1,-1)
);

void main() {
    localPos      = positions[gl_VertexIndex];
    gl_Position   = pc.proj * pc.view * vec4(localPos, 1.0);
}
