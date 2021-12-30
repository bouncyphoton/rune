#version 450

layout (location = 0) out VertexData {
    vec2 uv;
} VS_OUT;

layout (push_constant) uniform PerDrawConstants
{
    float time;
} u_push;

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
