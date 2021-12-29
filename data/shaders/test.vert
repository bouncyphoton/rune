#version 450

layout (set = 0, binding = 0) uniform VP {
    mat4 proj;
    mat4 view;
} u_vp;

layout (push_constant) uniform PerDrawConstants
{
    mat4 model;
    mat4 normal;
} u_push;

void main() {
    gl_Position = vec4(vec3(0.0), 1.0);
}
