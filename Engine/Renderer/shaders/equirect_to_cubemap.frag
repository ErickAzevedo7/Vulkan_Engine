#version 450

// Samples an equirectangular image in the direction of each cubemap texel.

layout(set = 0, binding = 0) uniform sampler2D equirectangularMap;

layout(location = 0) in vec3 fragDirection;

layout(location = 0) out vec4 outColor;

const vec2 kInverseAtan = vec2(0.1591, 0.3183); // 1/(2*PI), 1/PI

vec2 SampleSphericalMap(vec3 direction) {
    vec2 uv = vec2(atan(direction.z, direction.x), -asin(direction.y));
    uv *= kInverseAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(fragDirection));
    outColor = vec4(texture(equirectangularMap, uv).rgb, 1.0);
}
