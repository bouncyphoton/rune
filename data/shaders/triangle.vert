#version 450

struct Vertex {
    vec3 position;
    vec2 uv;
};

struct ObjectData {
    mat4 model_matrix;
};

layout (location = 0) out VertexData {
    vec2 uv;
    float object_id;
} VS_OUT;

layout (std430, set = 0, binding = 0) readonly buffer VertexBuffer {
    float data[];
} u_vertices;

layout (std430, set = 0, binding = 1) readonly buffer ObjectDataBuffer {
    ObjectData data[];
} u_object_data;

layout (push_constant) uniform PushConstants
{
    mat4 vp;
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
    uint object_id = gl_InstanceIndex;

    Vertex v = get_vertex(gl_VertexIndex);
    ObjectData o = u_object_data.data[object_id];

    vec4 position = u_push.vp * o.model_matrix * vec4(v.position, 1);
    VS_OUT.uv = v.uv;
    VS_OUT.object_id = object_id;

    gl_Position = position;
}
