// A unit cube as 36 positions indexed by gl_VertexIndex, so passes that draw a
// cube need no vertex buffer. Wound counter-clockwise seen from outside.

const vec3 kCubePositions[36] = vec3[](
    // +X
    vec3( 1,-1,-1), vec3( 1,-1, 1), vec3( 1, 1, 1),
    vec3( 1, 1, 1), vec3( 1, 1,-1), vec3( 1,-1,-1),
    // -X
    vec3(-1,-1, 1), vec3(-1,-1,-1), vec3(-1, 1,-1),
    vec3(-1, 1,-1), vec3(-1, 1, 1), vec3(-1,-1, 1),
    // +Y
    vec3(-1, 1,-1), vec3( 1, 1,-1), vec3( 1, 1, 1),
    vec3( 1, 1, 1), vec3(-1, 1, 1), vec3(-1, 1,-1),
    // -Y
    vec3(-1,-1, 1), vec3( 1,-1, 1), vec3( 1,-1,-1),
    vec3( 1,-1,-1), vec3(-1,-1,-1), vec3(-1,-1, 1),
    // +Z
    vec3(-1,-1, 1), vec3(-1, 1, 1), vec3( 1, 1, 1),
    vec3( 1, 1, 1), vec3( 1,-1, 1), vec3(-1,-1, 1),
    // -Z
    vec3( 1,-1,-1), vec3( 1, 1,-1), vec3(-1, 1,-1),
    vec3(-1, 1,-1), vec3(-1,-1,-1), vec3( 1,-1,-1)
);
