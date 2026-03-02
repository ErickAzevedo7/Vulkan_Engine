#version 450

layout(location = 0) in vec4 inPos;

layout(binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix[6];
    vec4 lightPos_farPlane;
} shadow;

void main() {
    float far_plane = shadow.lightPos_farPlane.w;

    if (far_plane > 0.0) {
        // Point light: omnidirectional linear depth
        vec3  lightPos  = shadow.lightPos_farPlane.xyz;
        float dist = length(inPos.xyz - lightPos) / far_plane;
        gl_FragDepth = dist;
    } else {
        // Directional light: standard perspective/ortho depth
        gl_FragDepth = gl_FragCoord.z;
    }
}
