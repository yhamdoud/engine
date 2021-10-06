#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_tex_coords;
layout (location = 3) in vec4 a_tangent;

uniform mat4 u_model;
uniform mat4 u_mvp;
uniform mat4 u_mvp_prev;
uniform mat3 u_normal_mat;

uniform mat4 u_light_transform;

out Varying
{
    vec3 normal;
    vec2 tex_coords;
    vec4 tangent;
    vec4 light_space_pos;
    vec4 position;
    vec4 position_prev;
} vs_out;

void main()
{
    vs_out.normal = u_normal_mat * a_normal;
    vs_out.tex_coords = a_tex_coords;
    vs_out.tangent.xyz = u_normal_mat * a_tangent.xyz;
    vs_out.tangent.w = a_tangent.w;

    vs_out.light_space_pos = u_light_transform * u_model * vec4(a_position, 1);

    vs_out.position = u_mvp * vec4(a_position, 1.);
    vs_out.position_prev = u_mvp_prev * vec4(a_position, 1.);

    gl_Position = vs_out.position;
}