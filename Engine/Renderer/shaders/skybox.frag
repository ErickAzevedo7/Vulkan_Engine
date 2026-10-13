#version 450
#extension GL_GOOGLE_include_directive : require

#include "frame_uniforms.glsl"
#include "environment_uniforms.glsl"

layout(location = 0) in vec3 fragDirection;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 radiance = texture(environmentMap, normalize(fragDirection)).rgb;
    outColor = vec4(radiance * frame.environmentParameters.x, 1.0);
}
