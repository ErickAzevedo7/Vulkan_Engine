#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragViewPos;
layout(binding = 1) uniform sampler2D texSampler;

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
} light;

layout(binding = 3) uniform MaterialProps {
    vec3 ambient;
    float shininess;
    vec3 specular;
    vec3 diffuse;
} material;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec3 pickColor;
    int usePickColor;
} pc;

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
    vec3 ambient = light.ambient.rgb * material.ambient * texColor;
    
    // diffuse - modulated by texture color
    vec3 norm = normalize(fragNormal);
    
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse.rgb * (diff * material.diffuse) * texColor;
    
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
    vec3 result = (ambient + diffuse + specular) * lightIntensity;
    outColor = vec4(result, tex.a);
    }
}