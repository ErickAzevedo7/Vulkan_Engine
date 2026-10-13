#version 450

// Builds a triangle that covers the screen from the vertex index alone,
// so passes using it need no vertex buffer.
//
// The RHI draws with Y up while textures start at their first row, so the clip
// position flips Y to keep UV (0, 0) at the top left of the image.

layout(location = 0) out vec2 fragUV;

void main() {
    fragUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);

    vec2 position = fragUV * 2.0 - 1.0;
    gl_Position = vec4(position.x, -position.y, 0.0, 1.0);
}
