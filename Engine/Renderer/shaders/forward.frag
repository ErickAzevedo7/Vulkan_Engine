#version 450
#extension GL_GOOGLE_include_directive : require

#include "frame_uniforms.glsl"
#include "environment_uniforms.glsl"
#include "material_uniforms.glsl"
#include "pbr_common.glsl"

// Writes linear color. The sRGB swapchain format encodes it for display.

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

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

// How far past the surface, in shadow map texels, the lookup point is moved along
// the normal. Keeps surfaces from shadowing themselves at grazing angles.
const float kShadowNormalOffset = 1.5;

// Fraction of light that reaches the surface, filtered over 5x5 texels.
float GetShadow(int shadowIndex, vec3 worldPosition, vec3 normal)
{
    Shadow shadow = frame.shadows[shadowIndex];
    float layer = shadow.parameters.y;

    // Texel size grows with distance for spot lights; clip w is 1 for directional ones.
    float texelWorldSize = shadow.parameters.x * (shadow.viewProjection * vec4(worldPosition, 1.0)).w;
    vec3 offsetPosition = worldPosition + normal * texelWorldSize * kShadowNormalOffset;

    vec4 clip = shadow.viewProjection * vec4(offsetPosition, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    // Clip space is Y-up, texture rows start at the top.
    vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (clip.w <= 0.0 || ndc.z > 1.0 || any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return 1.0;
    }

    vec2 texel = 1.0 / vec2(textureSize(shadowMaps, 0).xy);
    float lit = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            lit += texture(shadowMaps, vec4(uv + vec2(x, y) * texel, layer, ndc.z));
        }
    }
    return lit / 25.0;
}

// Must match kPointShadowNearPlane in private/ShaderLayouts.h.
const float kPointShadowNearPlane = 0.05;

// Directions spread around a point shadow lookup, used to filter it.
const vec3 kPointShadowOffsets[20] = vec3[](
    vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
    vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
    vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
    vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
    vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1));

// Fraction of a point light that reaches the surface, filtered over 20 lookups.
float GetPointShadow(float cube, vec3 lightPosition, float range, vec3 worldPosition, vec3 normal)
{
    float farPlane = max(range, kPointShadowNearPlane * 2.0);
    float faceSize = float(textureSize(pointShadowMaps, 0).x);

    // Each face covers 90 degrees, so a texel grows with the distance along the face's axis.
    vec3 toSurface = worldPosition - lightPosition;
    float axisDistance = max(abs(toSurface.x), max(abs(toSurface.y), abs(toSurface.z)));
    float texelWorldSize = 2.0 * axisDistance / faceSize;
    toSurface += normal * texelWorldSize * kShadowNormalOffset;

    if (axisDistance >= farPlane) {
        return 1.0;
    }

    vec3 surfaceSize = abs(toSurface);
    float radius = texelWorldSize * 1.5;
    float lit = 0.0;
    for (int i = 0; i < 20; ++i) {
        vec3 lookup = toSurface + kPointShadowOffsets[i] * radius;

        // Each face stores depth along its own axis, so the surface's reference depth is
        // measured along the axis of the face this lookup lands on.
        vec3 lookupSize = abs(lookup);
        float z = lookupSize.x > lookupSize.y && lookupSize.x > lookupSize.z ? surfaceSize.x
                : lookupSize.y > lookupSize.z                                ? surfaceSize.y
                                                                             : surfaceSize.z;
        z = max(z, kPointShadowNearPlane);
        float depth = farPlane * (z - kPointShadowNearPlane) / (z * (farPlane - kPointShadowNearPlane));

        lit += texture(pointShadowMaps, vec4(lookup, cube), depth);
    }
    return lit / 20.0;
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
    vec3 geometricNormal = normalize(fragNormal);
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

            if (type == LIGHT_SPOT) {
                float cosAngle = dot(-L, light.directionAndType.xyz);
                float cosInner = light.spotAndShadow.x;
                float cosOuter = light.spotAndShadow.y;
                float cone = clamp((cosAngle - cosOuter) / max(cosInner - cosOuter, 1e-4), 0.0, 1.0);
                radiance *= cone * cone;
            }
        }

        float shadowIndex = light.spotAndShadow.z;
        if (shadowIndex >= 0.0) {
            if (type == LIGHT_POINT) {
                radiance *= GetPointShadow(shadowIndex, light.positionAndRange.xyz, light.positionAndRange.w,
                                           fragWorldPosition, geometricNormal);
            } else {
                radiance *= GetShadow(int(shadowIndex + 0.5), fragWorldPosition, geometricNormal);
            }
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

    // Ambient light comes from the environment: its average color for the diffuse
    // half, and its blurred reflection for the specular half.
    vec3 ambientFresnel = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 ambientDiffuseFactor = (vec3(1.0) - ambientFresnel) * (1.0 - metallic);

    vec3 diffuseIBL = texture(irradianceMap, N).rgb * albedo;

    vec3 reflection = reflect(-V, N);
    vec3 prefiltered = textureLod(prefilterMap, reflection, roughness * kMaxReflectionLod).rgb;
    vec2 environmentBRDF = texture(brdfLut, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefiltered * (ambientFresnel * environmentBRDF.x + environmentBRDF.y);

    vec3 ambient = (ambientDiffuseFactor * diffuseIBL + specularIBL) * occlusion * frame.environmentParameters.x;
    outColor = vec4(ambient + Lo + emissive, albedoSample.a * material.baseColor.a);
}
