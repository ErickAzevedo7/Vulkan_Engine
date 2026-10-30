#version 450

layout(push_constant) uniform PickConstants {
    mat4 modelViewProjection;
    uvec2 id;
} pick;

layout(location = 0) out uvec2 outID;

void main() {
    outID = pick.id;
}
