#version 450
#extension GL_GOOGLE_include_directive : require

#include "ibl_common.glsl"

// Precomputes the scale and bias applied to reflectance at normal incidence.
// X axis is the angle to the normal, Y axis is roughness.

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec2 outBRDF;

const uint kSampleCount = 1024u;

// Geometry term with the remapping image based lighting uses.
float GeometrySchlickIBL(float NdotV, float roughness) {
    float k = (roughness * roughness) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

vec2 IntegrateBRDF(float NdotV, float roughness) {
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    float scale = 0.0;
    float bias = 0.0;

    for (uint i = 0u; i < kSampleCount; ++i) {
        vec2 random = Hammersley(i, kSampleCount);
        vec3 H = ImportanceSampleGGX(random, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            float G = GeometrySchlickIBL(NdotV, roughness) * GeometrySchlickIBL(NdotL, roughness);
            float visibility = (G * VdotH) / (NdotH * NdotV);
            float fresnel = pow(1.0 - VdotH, 5.0);

            scale += (1.0 - fresnel) * visibility;
            bias += fresnel * visibility;
        }
    }

    return vec2(scale, bias) / float(kSampleCount);
}

void main() {
    outBRDF = IntegrateBRDF(fragUV.x, fragUV.y);
}
