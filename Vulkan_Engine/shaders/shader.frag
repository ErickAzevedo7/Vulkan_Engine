#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragViewPos;
layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform Light {
    vec4 colorIntensity; // rgb=color, a=intensity
    vec4 direction;      // xyz=direction, w=pad
    vec4 positionType;   // xyz=position, w=type (0=directional,1=point)
} light;

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

        // read light data from uniform
        vec3 lightColor = light.colorIntensity.rgb;
        float lightIntensity = light.colorIntensity.a;
        vec3 lightDir = light.direction.xyz;
        vec3 lightPos = light.positionType.xyz;
        int lightType = int(light.positionType.w + 0.5);

        vec3 norm = normalize(fragNormal);
        vec3 lightDirection = normalize(lightPos - fragPosition);

        float diff = max(dot(norm, lightDirection), 0.0);
        vec3 diffuse = diff * lightColor * lightIntensity;

        vec3 ambient = 0.20 * tex.rgb;

        float specularStrength = 1.0f;
        vec3 viewDir = normalize(fragViewPos - fragPosition);
        vec3 reflectDir = reflect(-lightDirection, norm);
        
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = specularStrength * spec * lightColor;  

        vec3 color = (ambient + diffuse + specular) * tex.rgb;
        outColor = vec4(color, tex.a);
    }
}