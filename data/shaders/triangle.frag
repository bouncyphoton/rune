#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#include "defs.glsl"
#include "colors.glsl"

#define DRAW_OBJECT_ID 0

layout (location = 0) out vec4 o_img;

layout (location = 0) in VertexData {
    vec3 normal;
    vec2 uv;
    float object_id;
} FS_IN;

void main() {
    ObjectData o = u_object_data.data[uint(FS_IN.object_id)];

#if DRAW_OBJECT_ID
    o_img = vec4(get_color_for_float(float(FS_IN.object_id)), 1);
#else
    o_img = texture(u_textures[o.material_id], FS_IN.uv);
#endif
}
