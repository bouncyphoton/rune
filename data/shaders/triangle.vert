#version 450

struct Vertex {
    vec3 position;
    vec2 uv;
};

layout (location = 0) out VertexData {
    vec2 uv;
} VS_OUT;

layout (std430, set = 0, binding = 0) readonly buffer VertexBuffer {
    float data[];
} u_vertices;

layout (push_constant) uniform PerDrawConstants
{
    float time;
} u_push;

Vertex get_vertex(uint id) {
    Vertex v;
    v.position.x = u_vertices.data[id * 5 + 0];
    v.position.y = u_vertices.data[id * 5 + 1];
    v.position.z = u_vertices.data[id * 5 + 2];
    v.uv.x = u_vertices.data[id * 5 + 3];
    v.uv.y = u_vertices.data[id * 5 + 4];
    return v;
}

void main() {
    Vertex v = get_vertex(gl_VertexIndex);

    vec3 position = v.position;
    VS_OUT.uv = v.uv;

    position.y += sin(u_push.time + VS_OUT.uv.x) * 0.1;

    gl_Position = vec4(position, 1);
}

/*
void main() {
    const vec2 uvs[3] = vec2[3](
        vec2(1, 1),
        vec2(0.5, 0),
        vec2(0, 1)
    );

    VS_OUT.uv = uvs[gl_VertexIndex];

    vec2 position = VS_OUT.uv * 2.0 - 1.0;
    position *= 0.5;
    position.y += sin(u_push.time + VS_OUT.uv.x) * 0.1;

    gl_Position = vec4(position, 0.0f, 1.0f);
}
*/