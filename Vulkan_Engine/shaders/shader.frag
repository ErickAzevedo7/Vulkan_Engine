#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragViewPos;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 4) uniform sampler2D shadowSampler;
layout(binding = 5) uniform samplerCube shadowCubeSampler;
layout(binding = 6) uniform samplerCube irradianceMap;
layout(binding = 7) uniform samplerCube prefilterMap;
layout(binding = 8) uniform sampler2D   brdfLUT;

layout(binding = 2) uniform Light {
    vec4 colorIntensity; // rgb=color, a=intensity
    vec4 direction;      // xyz=direction, w=pad
    vec4 positionType;   // xyz=position, w=type (0=directional,1=point,2=spot)
    float attenuationKc;
    float attenuationKl;
    float attenuationKq;
    float cutOff;        // inner cone angle (cosine)
    float outerCutOff;   // outer cone angle (cosine)
    float far_plane;     // point light shadow far plane
    float _pad[2];
} light;

layout(binding = 3) uniform MaterialProps {
    vec4 albedo_pad; // xyz = albedo, w is padding
    float metallic;
    float roughness;
    float ao;
    float _pad;
} material;

layout(location = 0) out vec4 outColor;

// Layout must exactly match the C++ push constant struct.
// Offset 0: pickColor (vec3) + usePickColor (int)  = 16 bytes
// Offset 16: lightSpaceMatrix (mat4)               = 64 bytes  → total 80 bytes
layout(push_constant) uniform PushConstants {
    vec3 pickColor;
    int  usePickColor;
    mat4 lightSpaceMatrix; // directional light view-projection for shadow sampling
} pc;

const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
// PBR Functions (LearnOpenGL)
// ----------------------------------------------------------------------------

// Normal Distribution Function: Trowbridge-Reitz GGX
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Geometry Function: Schlick-GGX
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

// Geometry Function: Smith's method (combined)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel Equation: Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness factor (used for ambient IBL term)
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----------------------------------------------------------------------------
// Shadow Functions (unchanged)
// ----------------------------------------------------------------------------

// Directional/Spot light: PCF over a 2D shadow map
float shadowCalculation(vec3 fragPos, vec3 normal, vec3 lightDir)
{
    // Transform fragment position into light clip space
    vec4 fragPosLightSpace = pc.lightSpaceMatrix * vec4(fragPos, 1.0);

    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Map from [-1,1] to [0,1]
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float currentDepth = projCoords.z;

    // Keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0 || fragPosLightSpace.w < 0.0)
        return 0.0;

    // Slope-scaled bias
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);

    // 5×5 PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowSampler, 0);
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float pcfDepth = texture(shadowSampler, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 25.0;

    return shadow;
}

vec3 gridSamplingDisk[20] = vec3[](
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1),
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

// Point light: PCF over a cube shadow map
float shadowCubeCalculation(vec3 lightPos, vec3 fragPos)
{
    vec3 fragToLight    = fragPos - lightPos;
    float currentDepth  = length(fragToLight);

    float shadow      = 0.0;
    float bias        = 0.15;
    int   samples     = 20;
    float viewDist    = length(fragViewPos - fragPos);
    float diskRadius  = (1.0 + (viewDist / light.far_plane)) / 25.0;

    for (int i = 0; i < samples; ++i) {
        float closestDepth = texture(shadowCubeSampler, fragToLight + gridSamplingDisk[i] * diskRadius).r;
        closestDepth *= light.far_plane;
        if (currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
        
    return shadow;
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
void main() {
    if (pc.usePickColor == 1) {
        outColor = vec4(pc.pickColor, 1.0);
        return;
    }

    vec4 tex = texture(texSampler, fragTexCoord);
    vec3 albedo   = material.albedo_pad.rgb * tex.rgb;
    float metallic  = material.metallic;
    float roughness = material.roughness;
    float ao        = material.ao;

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(fragViewPos - fragPosition);

    // Calculate reflectance at normal incidence (F0)
    // For dielectrics use 0.04; for metals use the albedo color
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Read light data
    vec3  lightColor     = light.colorIntensity.rgb;
    float lightIntensity = light.colorIntensity.a;
    vec3  lightPos       = light.positionType.xyz;
    int   lightType      = int(light.positionType.w + 0.5);

    // Determine light direction based on type
    vec3 L = vec3(0.0);
    if (lightType == 0) {
        L = normalize(-light.direction.xyz); // directional
    } else {
        L = normalize(lightPos - fragPosition); // point or spot
    }

    vec3 H = normalize(V + L);

    // Radiance
    vec3 radiance = lightColor * lightIntensity;

    // Attenuation for point/spot lights
    if (lightType == 1 || lightType == 2) {
        float distance    = length(lightPos - fragPosition);
        float attenuation = 1.0 / (distance * distance);
        radiance *= attenuation;
    }

    // Spotlight intensity
    if (lightType == 2) {
        vec3 spotDir = normalize(-light.direction.xyz);
        float theta   = dot(L, spotDir);
        float epsilon = light.cutOff - light.outerCutOff;
        float spotIntensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
        radiance *= spotIntensity;
    }

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular     = numerator / denominator;

    // Energy conservation: kS is Fresnel, kD is the remaining
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    // Metals have no diffuse component
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    // Shadow
    float shadow = 0.0;
    if (lightType == 1) { // Point light
        shadow = shadowCubeCalculation(lightPos, fragPosition);
    } else if (lightType == 0 || lightType == 2) { // Directional or Spot
        shadow = shadowCalculation(fragPosition, N, L);
    }

    // Outgoing radiance Lo
    vec3 Lo = (1.0 - shadow) * (kD * albedo / PI + specular) * radiance * NdotL;

    // Ambient lighting — IBL diffuse + specular (split-sum approximation)
    // Fresnel for ambient uses N·V and roughness, not H·V
    vec3 kS_ambient = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD_ambient = vec3(1.0) - kS_ambient;
    kD_ambient *= 1.0 - metallic; // Pure metals have ZERO diffuse reflection

    // --- Diffuse IBL ---
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse    = irradiance * albedo;

    // --- Specular IBL (split-sum) ---
    // Reflect view vector to look up environment in specular direction
    vec3 R = reflect(-V, N);
    const float MAX_REFLECTION_LOD = 4.0; // 5 mip levels (0..4)
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    // BRDF LUT encodes (scale, bias) for F0: X = NdotV axis, Y = roughness axis
    vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (kS_ambient * brdf.x + brdf.y);

    vec3 ambient    = (kD_ambient * diffuse + specularIBL) * ao;

    vec3 color = ambient + Lo;

    outColor = vec4(color, tex.a);
}