#version 450

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

struct ObjectData {
    mat4 model_matrix;
};

layout (location = 0) out VertexData {
    vec3 normal;
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
    int num_floats = 3 + 3 + 2;

    Vertex v;
    v.position.x = u_vertices.data[id * num_floats + 0];
    v.position.y = u_vertices.data[id * num_floats + 1];
    v.position.z = u_vertices.data[id * num_floats + 2];
    v.normal.x   = u_vertices.data[id * num_floats + 3];
    v.normal.y   = u_vertices.data[id * num_floats + 4];
    v.normal.z   = u_vertices.data[id * num_floats + 5];
    v.uv.x       = u_vertices.data[id * num_floats + 6];
    v.uv.y       = u_vertices.data[id * num_floats + 7];
    return v;
}

void main() {
    uint object_id = gl_InstanceIndex;

    Vertex v = get_vertex(gl_VertexIndex);
    ObjectData o = u_object_data.data[object_id];

    vec4 position = u_push.vp * o.model_matrix * vec4(v.position, 1);

    VS_OUT.normal    = v.normal;
    VS_OUT.uv        = v.uv;
    VS_OUT.object_id = object_id;

    gl_Position = position;
}
