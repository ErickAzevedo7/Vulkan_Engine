#version 450
#extension GL_GOOGLE_include_directive : require

#include "pbr_common.glsl"

// Averages the environment over the hemisphere around each direction, giving the
// diffuse light arriving from every side.

layout(set = 0, binding = 0) uniform samplerCube environmentMap;

layout(location = 0) in vec3 fragDirection;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragDirection);

    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    vec3 irradiance = vec3(0.0);
    float sampleCount = 0.0;
    const float kSampleDelta = 0.025;

    for (float phi = 0.0; phi < 2.0 * PI; phi += kSampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += kSampleDelta) {
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleDirection = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            irradiance += texture(environmentMap, sampleDirection).rgb * cos(theta) * sin(theta);
            sampleCount += 1.0;
        }
    }

    outColor = vec4(PI * irradiance / sampleCount, 1.0);
}
