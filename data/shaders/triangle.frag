#version 450
#include "colors.glsl"

#define DRAW_OBJECT_ID 1

layout (location = 0) out vec4 o_img;

layout (location = 0) in VertexData {
    vec2 uv;
    float object_id;
} FS_IN;

void main() {
#if DRAW_OBJECT_ID
    o_img = vec4(get_color_for_float(FS_IN.object_id), 1);
#else
    o_img = vec4(FS_IN.uv, 0.5, 1);
#endif
}
