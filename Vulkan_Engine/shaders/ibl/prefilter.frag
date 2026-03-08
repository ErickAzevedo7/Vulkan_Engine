#version 450

layout(location = 0) in  vec3 localPos;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform samplerCube environmentMap;

layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
    float roughness; // 0.0 (sharp) → 1.0 (fully blurred)
} pc;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// Karis average weight: suppresses bright outliers (e.g. sun disc) proportionally
// to their luminance, without any hardcoded cap value.
float luminance(vec3 v) {
    return dot(v, vec3(0.2126, 0.7152, 0.0722));
}

// ----------------------------------------------------------------------------
// Hammersley low-discrepancy sequence (generates well-distributed sample points)
// ----------------------------------------------------------------------------
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// ----------------------------------------------------------------------------
// GGX importance sampling: biases samples toward specular lobe
// ----------------------------------------------------------------------------
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;

    // Spherical-to-Cartesian in tangent space
    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // Transform from tangent space to world space
    vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

void main() {
    vec3 N = normalize(localPos);
    vec3 R = N;
    vec3 V = R;

    const uint  SAMPLE_COUNT = 5120u;

    float totalWeight      = 0.0;
    vec3  prefilteredColor = vec3(0.0);

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, pc.roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            float D = DistributionGGX(N, H, pc.roughness);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            // PDF of the importance sample
            float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;

            // Source env cubemap resolution and how many mip levels it has
            const float ENV_RESOLUTION = 512.0;

            // Solid angle of one texel in the source cube face
            float saTexel  = 4.0 * PI / (6.0 * ENV_RESOLUTION * ENV_RESOLUTION);
            // Solid angle covered by this sample
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

            float mipLevel = pc.roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

            vec3  envSample = textureLod(environmentMap, L, mipLevel).rgb;
            // Karis weight blended by roughness:
            //   roughness = 0 (mirror) → pure NdotL, no energy loss, sun shows correctly
            //   roughness = 1 (matte)  → full Karis, fireflies spread and suppressed
            float karisWeight = NdotL / mix(1.0, 1.0 + luminance(envSample), pc.roughness);
            prefilteredColor += envSample * karisWeight;
            totalWeight      += karisWeight;
        }
    }

    prefilteredColor = prefilteredColor / max(totalWeight, 0.0001);
    outColor = vec4(prefilteredColor, 1.0);
}
