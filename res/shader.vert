#version 450
#pragma shader_stage(vertex)

layout(location = 0) out vec2 f_uv;

layout(location = 0) in vec2 pos_in;
layout(location = 1) in vec2 uv_in;

void main() {
    gl_Position = vec4(pos_in, 0.0, 1.0);
    f_uv = uv_in;
}
