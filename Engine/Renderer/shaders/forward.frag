#version 450
#extension GL_GOOGLE_include_directive : require

#include "frame_uniforms.glsl"
#include "light_buffers.glsl"
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

// How much light gets past the depth stored at `uv`, a position inside the tile `rect`. Lookups
// stay half a texel inside it, as filtering would read the neighbor tile from past that.
float SampleAtlas(vec4 rect, vec2 uv, float depth)
{
    vec2 halfTexel = 0.5 / vec2(textureSize(shadowAtlas, 0));
    vec2 atlasUV = clamp(rect.xy + uv * rect.zw, rect.xy + halfTexel, rect.xy + rect.zw - halfTexel);
    return texture(shadowAtlas, vec3(atlasUV, depth));
}

// Fraction of light that reaches the surface, filtered over 5x5 texels.
float GetShadow(int shadowIndex, vec3 worldPosition, vec3 normal)
{
    mat4 viewProjection = shadows[shadowIndex].viewProjection;
    vec4 rect = shadows[shadowIndex].atlasRect;

    // Texel size grows with distance for spot lights; clip w is 1 for directional ones.
    float texelWorldSize = shadows[shadowIndex].parameters.x * (viewProjection * vec4(worldPosition, 1.0)).w;
    vec3 offsetPosition = worldPosition + normal * texelWorldSize * kShadowNormalOffset;

    vec4 clip = viewProjection * vec4(offsetPosition, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    // Clip space is Y-up, texture rows start at the top.
    vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (clip.w <= 0.0 || ndc.z > 1.0 || any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return 1.0;
    }

    // One texel of the tile, in the tile's own UVs.
    vec2 texel = 1.0 / (vec2(textureSize(shadowAtlas, 0)) * rect.zw);
    float lit = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            lit += SampleAtlas(rect, uv + vec2(x, y) * texel, ndc.z);
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

// Fraction of a point light that reaches the surface, filtered over 20 lookups. `firstFace` is
// the index of the light's +X face in shadows[].
float GetPointShadow(int firstFace, vec3 lightPosition, float range, vec3 worldPosition, vec3 normal)
{
    float farPlane = max(range, kPointShadowNearPlane * 2.0);

    // Each face covers 90 degrees, so a texel grows with the distance along the face's axis.
    vec3 toSurface = worldPosition - lightPosition;
    float axisDistance = max(abs(toSurface.x), max(abs(toSurface.y), abs(toSurface.z)));
    float texelWorldSize = shadows[firstFace].parameters.x * axisDistance;
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
        int face;
        float z;
        if (lookupSize.x > lookupSize.y && lookupSize.x > lookupSize.z) {
            face = lookup.x < 0.0 ? 1 : 0;
            z = surfaceSize.x;
        } else if (lookupSize.y > lookupSize.z) {
            face = lookup.y < 0.0 ? 3 : 2;
            z = surfaceSize.y;
        } else {
            face = lookup.z < 0.0 ? 5 : 4;
            z = surfaceSize.z;
        }
        z = max(z, kPointShadowNearPlane);
        float depth = farPlane * (z - kPointShadowNearPlane) / (z * (farPlane - kPointShadowNearPlane));

        vec4 clip = shadows[firstFace + face].viewProjection * vec4(lightPosition + lookup, 1.0);
        vec2 ndc = clip.xy / clip.w;
        lit += SampleAtlas(shadows[firstFace + face].atlasRect, vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5), depth);
    }
    return lit / 20.0;
}

// What a surface needs to answer to a light.
struct Surface {
    vec3 N;
    vec3 geometricNormal;
    vec3 V;
    vec3 albedo;
    vec3 F0;
    float metallic;
    float roughness;
};

vec3 GetLightColor(Light light, Surface surface)
{
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

    // Clusters are boxes, so many pixels in one are out of a light's reach.
    float NdotL = max(dot(surface.N, L), 0.0);
    if (NdotL <= 0.0 || dot(radiance, radiance) <= 0.0) {
        return vec3(0.0);
    }

    float shadowIndex = light.spotAndShadow.z;
    if (shadowIndex >= 0.0) {
        if (type == LIGHT_POINT) {
            radiance *= GetPointShadow(int(shadowIndex + 0.5), light.positionAndRange.xyz, light.positionAndRange.w,
                                       fragWorldPosition, surface.geometricNormal);
        } else {
            radiance *= GetShadow(int(shadowIndex + 0.5), fragWorldPosition, surface.geometricNormal);
        }
    }

    vec3 H = normalize(surface.V + L);

    float NDF = DistributionGGX(surface.N, H, surface.roughness);
    float G = GeometrySmith(surface.N, surface.V, L, surface.roughness);
    vec3 F = FresnelSchlick(max(dot(H, surface.V), 0.0), surface.F0);

    vec3 specular = (NDF * G * F) / (4.0 * max(dot(surface.N, surface.V), 0.0) * NdotL + 0.0001);

    // Metals have no diffuse reflection.
    vec3 kD = (vec3(1.0) - F) * (1.0 - surface.metallic);

    return (kD * surface.albedo / PI + specular) * radiance * NdotL;
}

// The cluster this pixel is in.
uint GetCluster()
{
    uvec3 counts = frame.clusterCounts.xyz;
    float depth = -(frame.view * vec4(fragWorldPosition, 1.0)).z;
    float slice = log(max(depth, 1e-4)) * frame.clusterScale.z + frame.clusterScale.w;

    uvec2 tile = min(uvec2(gl_FragCoord.xy * frame.clusterScale.xy), counts.xy - 1u);
    uint depthSlice = min(uint(max(slice, 0.0)), counts.z - 1u);
    return tile.x + counts.x * (tile.y + counts.y * depthSlice);
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
    Surface surface = Surface(N, geometricNormal, V, albedo, F0, metallic, roughness);

    // Directional lights reach every pixel; the others, only the clusters they touch.
    vec3 Lo = vec3(0.0);
    for (uint i = 0u; i < frame.lightCounts.x; ++i) {
        Lo += GetLightColor(lights[i], surface);
    }
    uvec2 cluster = clusters[GetCluster()];
    for (uint i = 0u; i < cluster.y; ++i) {
        Lo += GetLightColor(lights[lightIndices[cluster.x + i]], surface);
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
