#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(set = 0, binding = 0) uniform sampler2D u_IDImage;

layout(push_constant) uniform PushConstant {
    int selectedID;
} pc;

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    ivec2 texSize = textureSize(u_IDImage, 0);
    vec2 texel = 1.0 / vec2(texSize);

    float center = texture(u_IDImage, v_TexCoord).r;
    float up     = texture(u_IDImage, v_TexCoord + vec2(0.0, texel.y)).r;
    float down   = texture(u_IDImage, v_TexCoord - vec2(0.0, texel.y)).r;
    float left   = texture(u_IDImage, v_TexCoord - vec2(texel.x, 0.0)).r;
    float right  = texture(u_IDImage, v_TexCoord + vec2(texel.x, 0.0)).r;

    int idCenter = int(round(center * 255.0));
    int idUp     = int(round(up * 255.0));
    int idDown   = int(round(down * 255.0));
    int idLeft   = int(round(left * 255.0));
    int idRight  = int(round(right * 255.0));

    bool isSelected = (idCenter == pc.selectedID);
    bool edge = false;
    if (isSelected) {
        edge = edge || (idUp != pc.selectedID);
        edge = edge || (idDown != pc.selectedID);
        edge = edge || (idLeft != pc.selectedID);
        edge = edge || (idRight != pc.selectedID);
    }

    outColor = edge ? vec4(1.0, 1.0, 0.0, 1.0) : vec4(0.0);
}