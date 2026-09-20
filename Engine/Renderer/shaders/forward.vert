#version 450
#extension GL_GOOGLE_include_directive : require

#include "frame_uniforms.glsl"

// Must match ObjectPushConstants in private/ShaderLayouts.h.
layout(push_constant) uniform ObjectPushConstants {
    mat4 model;
    vec4 normalMatrix[3];
} object;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 fragWorldPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec4 fragTangent;

void main() {
    vec4 worldPosition = object.model * vec4(inPosition, 1.0);
    mat3 normalMatrix = mat3(object.normalMatrix[0].xyz, object.normalMatrix[1].xyz, object.normalMatrix[2].xyz);

    fragWorldPosition = worldPosition.xyz;
    fragNormal = normalMatrix * inNormal;
    fragTangent = vec4(mat3(object.model) * inTangent.xyz, inTangent.w);
    fragUV = inUV;

    gl_Position = frame.projection * frame.view * worldPosition;
}
