#version 450

layout (location = 0) out vec4 o_diffuse;
layout (location = 1) out vec4 o_normal;
layout (location = 2) out vec4 o_occlusion_roughness_metallic;

layout (set = 1, binding = 0) uniform sampler2D u_diffuse_texture;
layout (set = 1, binding = 1) uniform sampler2D u_normal_texture;
layout (set = 1, binding = 2) uniform sampler2D u_occlusion_texture;
layout (set = 1, binding = 3) uniform sampler2D u_roughness_texture;
layout (set = 1, binding = 4) uniform sampler2D u_metallic_texture;

void main() {
}
