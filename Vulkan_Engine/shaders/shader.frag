#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec3 pickColor;
    int usePickColor;
} pc;

void main() {
    if (pc.usePickColor == 1) 
        outColor = vec4(pc.pickColor, 1.0);
    else
        outColor = texture(texSampler, fragTexCoord);
}