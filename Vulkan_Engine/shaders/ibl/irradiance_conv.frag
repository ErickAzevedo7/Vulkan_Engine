#version 450

layout(location = 0) in  vec3 localPos;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main() {
    // The normal direction is simply the direction this fragment maps to on the cube
    vec3 N = normalize(localPos);

    // Tangent space vectors
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));

    // Riemann sum hemisphere convolution (LearnOpenGL approach)
    vec3  irradiance    = vec3(0.0);
    float sampleDelta   = 0.025;
    float nrSamples     = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // Spherical to Cartesian (tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // Transform to world space
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }

    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    outColor   = vec4(irradiance, 1.0);
}
