#version 450
#pragma shader_stage(fragment)
#define DIST_COEFF 0.0032

layout(location = 0) in vec2 f_uv;

layout(location = 0) out vec4 color_out;

vec3 colors[3] = vec3[](
    vec3(6.0, 0.0, 0.0),
    vec3(0.0, 6.0, 0.0),
    vec3(0.0, 0.0, 6.0)
);

vec2 points[3] = vec2[](
    vec2(-0.125, 0.125),
    vec2(0.125, 0.125),
    vec2(0.0, -0.125)
);

vec3 isl(vec2 p, vec2 lp, vec3 lc) {
    return lc / (dot(p-lp, p-lp) / DIST_COEFF);
}

void main() {
    vec3 c = isl(f_uv, points[0], colors[0]) + isl(f_uv, points[1], colors[1]) + isl(f_uv, points[2], colors[2]);
    color_out = vec4(c, 1.0);

}