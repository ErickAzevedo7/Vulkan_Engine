#version 450
#extension GL_GOOGLE_include_directive : require

#include "frame_uniforms.glsl"
#include "material_uniforms.glsl"
#include "pbr_common.glsl"

// Writes linear color. The sRGB swapchain format encodes it for display.

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

// Stands in for environment lighting.
const vec3 AMBIENT_COLOR = vec3(0.03);

vec3 GetNormal()
{
    vec3 N = normalize(fragNormal);

    // Tangent made orthogonal to the interpolated normal.
    vec3 T = fragTangent.xyz - dot(fragTangent.xyz, N) * N;
    float tangentLength = length(T);
    if (tangentLength < 1e-5) {
        return N;
    }
    T /= tangentLength;
    vec3 B = cross(N, T) * fragTangent.w;

    vec3 tangentNormal = texture(normalMap, fragUV).xyz * 2.0 - 1.0;
    return normalize(mat3(T, B, N) * tangentNormal);
}

// Inverse-square falloff that reaches zero at the light's range.
float GetAttenuation(float distance, float range)
{
    float ratio = distance / max(range, 1e-4);
    float window = clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);
    return (window * window) / max(distance * distance, 1e-4);
}

void main()
{
    vec4 albedoSample = texture(albedoMap, fragUV);
    vec3 albedo = albedoSample.rgb * material.baseColor.rgb;

    // Metallic is read from blue and roughness from green, which matches packed glTF maps
    // and works the same for single-channel grayscale maps.
    float metallic = clamp(texture(metallicMap, fragUV).b * material.metallicRoughness.x, 0.0, 1.0);
    float roughness = clamp(texture(roughnessMap, fragUV).g * material.metallicRoughness.y, 0.04, 1.0);
    float occlusion = mix(1.0, texture(occlusionMap, fragUV).r, material.emissiveAndOcclusion.w);
    vec3 emissive = texture(emissiveMap, fragUV).rgb * material.emissiveAndOcclusion.rgb;

    vec3 N = GetNormal();
    vec3 V = normalize(frame.cameraPosition.xyz - fragWorldPosition);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    for (uint i = 0u; i < frame.lightCount.x; ++i) {
        Light light = frame.lights[i];
        int type = int(light.directionAndType.w + 0.5);

        vec3 radiance = light.colorAndIntensity.rgb * light.colorAndIntensity.a;
        vec3 L;
        if (type == LIGHT_DIRECTIONAL) {
            L = -light.directionAndType.xyz;
        } else {
            vec3 toLight = light.positionAndRange.xyz - fragWorldPosition;
            float distance = length(toLight);
            L = toLight / max(distance, 1e-4);
            radiance *= GetAttenuation(distance, light.positionAndRange.w);
        }

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001);

        // Metals have no diffuse reflection.
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = AMBIENT_COLOR * albedo * occlusion;
    outColor = vec4(ambient + Lo + emissive, albedoSample.a * material.baseColor.a);
}
