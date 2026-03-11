#version 450

// Per-camera global data — updated once per render pass (Editor or Game)
// Set 0 is swapped per-viewport at draw time
layout(set=0, binding=0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrices[6];
    vec4 lightPos_farPlane;
    vec3 viewPos;
} global;

// Per-entity data — dynamic UBO (one slot per entity), in the material set
layout(set=1, binding=9) uniform PerObjectUBO {
    mat4 model;
    mat4 normal;
} obj;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragViewPos;

void main() {
    gl_Position = global.proj * global.view * obj.model * vec4(inPosition, 1.0);
    fragPosition = vec3(obj.model * vec4(inPosition, 1.0));
    fragNormal = mat3(obj.normal) * inNormal;
    fragTexCoord = inTexCoord;
    fragViewPos = global.viewPos;
}