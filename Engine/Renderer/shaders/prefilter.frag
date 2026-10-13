#version 450
#extension GL_GOOGLE_include_directive : require

#include "ibl_common.glsl"

// Blurs the environment by roughness, one mip level per step, for the specular
// half of image based lighting.

layout(set = 0, binding = 0) uniform samplerCube environmentMap;

layout(push_constant) uniform CaptureConstants {
    mat4 viewProjection;
    // x = roughness of this mip level
    vec4 parameters;
} capture;

layout(location = 0) in vec3 fragDirection;

layout(location = 0) out vec4 outColor;

const uint kSampleCount = 1024u;
const float kSourceResolution = 512.0;

float Luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    float roughness = capture.parameters.x;

    vec3 N = normalize(fragDirection);
    vec3 V = N;

    vec3 prefiltered = vec3(0.0);
    float totalWeight = 0.0;

    for (uint i = 0u; i < kSampleCount; ++i) {
        vec2 random = Hammersley(i, kSampleCount);
        vec3 H = ImportanceSampleGGX(random, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) continue;

        float D = DistributionGGX(N, H, roughness);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;

        // Picks a source mip wide enough for how much of the sphere this sample covers.
        float texelSolidAngle = 4.0 * PI / (6.0 * kSourceResolution * kSourceResolution);
        float sampleSolidAngle = 1.0 / (float(kSampleCount) * pdf + 0.0001);
        float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(sampleSolidAngle / texelSolidAngle);

        vec3 sampled = textureLod(environmentMap, L, mipLevel).rgb;

        // Weighs very bright samples down with roughness, so a small sun does not
        // turn into scattered dots on the blurred mips.
        float weight = NdotL / mix(1.0, 1.0 + Luminance(sampled), roughness);
        prefiltered += sampled * weight;
        totalWeight += weight;
    }

    outColor = vec4(prefiltered / max(totalWeight, 0.0001), 1.0);
}
