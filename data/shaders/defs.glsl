struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

struct ObjectData {
    mat4 model_matrix;
    uint material_id;
};

layout (std430, set = 0, binding = 0) readonly buffer VertexBuffer {
    float data[];
} u_vertices;

layout (std430, set = 0, binding = 1) readonly buffer ObjectDataBuffer {
    ObjectData data[];
} u_object_data;

layout (set = 1, binding = 0) uniform sampler2D u_textures[128];
