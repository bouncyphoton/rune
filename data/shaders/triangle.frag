#version 450

layout (location = 0) out vec4 o_img;

layout (location = 0) in VertexData {
    vec2 uv;
} FS_IN;

void main() {
    o_img = vec4(FS_IN.uv, 0.5, 1);
}
