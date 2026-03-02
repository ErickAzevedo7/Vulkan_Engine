#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragViewPos;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 4) uniform sampler2D shadowSampler;
layout(binding = 5) uniform samplerCube shadowCubeSampler;

layout(binding = 2) uniform Light {
    vec4 colorIntensity; // rgb=color, a=intensity
    vec4 direction;      // xyz=direction, w=pad
    vec4 positionType;   // xyz=position, w=type (0=directional,1=point,2=spot)
    vec4 ambient;        // rgb=ambient color
    vec4 diffuse;        // rgb=diffuse color
    vec4 specular;       // rgb=specular color
    float attenuationKc;
    float attenuationKl;
    float attenuationKq;
    float cutOff;        // inner cone angle (cosine)
    float outerCutOff;   // outer cone angle (cosine)
    int useBlinnPhong;   // 1 = Blinn-Phong, 0 = Phong
    float far_plane;     // point light shadow far plane
} light;

layout(binding = 3) uniform MaterialProps {
    vec3 ambient;
    float shininess;
    vec3 specular;
    vec3 diffuse;
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

// ----------------------------------------------------------------------------
// Directional light: PCF over a 2D shadow map
// ----------------------------------------------------------------------------
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
    // Additionally, prevent back-projection (w < 0.0) where geometry behind the spotlight is erroneously shadowed.
    if(projCoords.z > 1.0 || fragPosLightSpace.w < 0.0)
        return 0.0;
    // Slope-scaled bias to prevent shadow acne, reduced to stop peter-panning
    // (Vulkan pipeline already uses front-face culling for shadows)
    float bias = max(0.001 * (1.0 - dot(normal, lightDir)), 0.0001);

    // 3×3 PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowSampler, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowSampler, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

vec3 gridSamplingDisk[20] = vec3[](
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1),
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

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

void main() {
    if (pc.usePickColor == 1)
        outColor = vec4(pc.pickColor, 1.0);
    else
    {
    vec4 tex = texture(texSampler, fragTexCoord);
    vec3 texColor = tex.rgb;

    // read light data from uniform
    vec3 lightColor = light.colorIntensity.rgb;
    float lightIntensity = light.colorIntensity.a;
    vec3 lightDir = light.direction.xyz;
    vec3 lightPos = light.positionType.xyz;
    int lightType = int(light.positionType.w + 0.5);

    if (lightType == 0) {
        lightDir = normalize(-light.direction.xyz);
    } else if (lightType == 1 || lightType == 2) {
        lightDir = normalize(lightPos - fragPosition);
    }

    // Spotlight intensity calculation
    float spotIntensity = 1.0;
    if (lightType == 2) {
        vec3 spotDir = normalize(-light.direction.xyz);
        float theta = dot(lightDir, spotDir);
        float epsilon = light.cutOff - light.outerCutOff;
        spotIntensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    }

    //ambient - modulated by texture color
    vec3 ambient = light.ambient.rgb * material.ambient;

    // diffuse - modulated by texture color
    vec3 norm = normalize(fragNormal);
    
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse.rgb * (diff * material.diffuse);

    // specular (Blinn-Phong or Phong based on flag) - NOT modulated by texture color
    vec3 viewDir = normalize(fragViewPos - fragPosition);
    float spec;
    
    if (light.useBlinnPhong == 1) {
        // Blinn-Phong: use halfway vector
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(norm, halfwayDir), 0.0), material.shininess);
    } else {
        // Phong: use reflection vector
        vec3 reflectDir = reflect(-lightDir, norm);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    }
    
    vec3 specular = light.specular.rgb * (spec * material.specular);

    // point light and spotlight attenuation
    if (lightType == 1 || lightType == 2) {
      float distance = length(lightPos - fragPosition);
      float attenuation = 1.0 / (light.attenuationKc + light.attenuationKl * distance + light.attenuationKq * (distance * distance));

        ambient  *= attenuation;
        diffuse  *= attenuation;
        specular *= attenuation;
    }

    // Apply spotlight intensity
    if (lightType == 2) {
        diffuse  *= spotIntensity;
        specular *= spotIntensity;
    }

    // Final result - apply light color and intensity
    float shadow = 0.0;
    if (lightType == 1) { // Point light
        shadow = shadowCubeCalculation(lightPos, fragPosition);
    } else if (lightType == 0 || lightType == 2) { // Directional or Spot light
        shadow = shadowCalculation(fragPosition, norm, lightDir);
    }

    vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * texColor * lightIntensity;
    outColor = vec4(result, tex.a);
    }
}