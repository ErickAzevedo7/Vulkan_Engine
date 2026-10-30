#version 450

// Shapes in world coordinates, over a scene that is already displayable color. Lines arrive as
// four vertices per segment and are widened here, so their width is in pixels.

// Must match WorldConstants and the flags in private/WorldPass.cpp.
layout(push_constant) uniform WorldConstants {
    mat4 viewProjection;
    vec2 viewportSize;
    // Start and end of the fade, in distance from the camera.
    vec2 fade;
    vec3 cameraPosition;
    uint flags;
} world;

const uint kLines = 1u;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inOther;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in float inWidth;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec3 fragWorldPosition;
// Lines: pixels from the segment's middle, and half its width.
layout(location = 3) out float fragEdge;
layout(location = 4) out float fragHalfWidth;

// Pulls shapes a little toward the camera, by a share of their distance, so lines lying on a
// surface aren't hidden by it.
vec4 PullTowardCamera(vec4 position) {
    float depth = position.z / position.w;
    depth -= 0.002 * (1.0 - depth);
    return vec4(position.xy, depth * position.w, position.w);
}

void main() {
    fragUV = inUV;
    fragColor = inColor;
    fragWorldPosition = inPosition;
    fragEdge = 0.0;
    fragHalfWidth = 0.0;

    vec4 self = world.viewProjection * vec4(inPosition, 1.0);
    if ((world.flags & kLines) == 0u) {
        gl_Position = PullTowardCamera(self);
        return;
    }

    // A segment passing behind the camera is cut at the near plane, so it still has a
    // direction on screen.
    vec4 other = world.viewProjection * vec4(inOther, 1.0);
    if (self.z < 0.0 && other.z < 0.0) {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
        return;
    }
    vec4 a = self;
    vec4 b = other;
    if (a.z < 0.0) a = mix(self, other, self.z / (self.z - other.z));
    if (b.z < 0.0) b = mix(other, self, other.z / (other.z - self.z));

    vec2 halfViewport = world.viewportSize * 0.5;
    vec2 direction = b.xy / b.w * halfViewport - a.xy / a.w * halfViewport;
    direction = length(direction) > 1e-5 ? normalize(direction) : vec2(1.0, 0.0);
    // Both ends must offset to the same side.
    if (inUV.y > 0.5) direction = -direction;
    vec2 normal = vec2(-direction.y, direction.x);

    // One more pixel on each side for the soft edge.
    float halfWidth = inWidth * 0.5;
    float extent = halfWidth + 1.0;
    a.xy += normal * (inUV.x * extent) / halfViewport * a.w;

    fragEdge = inUV.x * extent;
    fragHalfWidth = halfWidth;
    gl_Position = PullTowardCamera(a);
}
