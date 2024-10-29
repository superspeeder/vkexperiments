#version 450
#pragma shader_stage(vertex)

layout(location = 0) out vec2 p;

vec2 positions[3] = vec2[](
vec2(-3.0, -1.0),
vec2(1.0, -1.0),
vec2(1.0, 3.0)
);

void main() {
    p = positions[gl_VertexIndex].xy;
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
