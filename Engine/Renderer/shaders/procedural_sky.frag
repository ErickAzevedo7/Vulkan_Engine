#version 450

// Stand-in sky for scenes with no environment image: a gradient from ground to
// zenith plus a bright sun, in high dynamic range so it can light the scene.

layout(location = 0) in vec3 fragDirection;

layout(location = 0) out vec4 outColor;

const vec3 kGroundColor = vec3(0.16, 0.14, 0.12);
const vec3 kHorizonColor = vec3(0.75, 0.80, 0.90);
const vec3 kZenithColor = vec3(0.12, 0.28, 0.65);
const vec3 kSunColor = vec3(14.0, 12.0, 9.5);
const vec3 kSunDirection = vec3(0.4, 0.85, 0.6);

void main() {
    vec3 direction = normalize(fragDirection);

    vec3 color;
    if (direction.y >= 0.0) {
        color = mix(kHorizonColor, kZenithColor, pow(direction.y, 0.45));
    } else {
        color = mix(kHorizonColor, kGroundColor, min(-direction.y * 3.0, 1.0));
    }

    float sun = pow(max(dot(direction, normalize(kSunDirection)), 0.0), 900.0);
    color += kSunColor * sun;

    outColor = vec4(color, 1.0);
}
