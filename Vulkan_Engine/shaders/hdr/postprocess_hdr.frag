#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D hdrBuffer;

layout(push_constant) uniform PushConstants {
    float exposure;
} pc;

void main() {
    vec3 hdrColor = texture(hdrBuffer, inTexCoord).rgb;
  
    // Exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-hdrColor * pc.exposure);
  
    outColor = vec4(mapped, 1.0);
}
