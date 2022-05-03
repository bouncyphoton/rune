#version 450
#include "colors.glsl"

#define DRAW_OBJECT_ID 0

layout (location = 0) out vec4 o_img;

layout (location = 0) in VertexData {
    vec3 normal;
    vec2 uv;
    float object_id;
} FS_IN;

layout (set = 1, binding = 3) uniform sampler2D u_textures[];

void main() {
#if DRAW_OBJECT_ID
    o_img = vec4(get_color_for_float(FS_IN.object_id), 1);
#else
    //o_img = vec4(FS_IN.normal * 0.5f + 0.5f, 1);
    o_img = texture(u_textures[0], FS_IN.uv);
#endif
}
