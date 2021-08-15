#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_tex_coords;

uniform mat4 u_model_view;
uniform mat4 u_mvp;
uniform mat3 u_normal_mat;

out Varying
{
	vec3 position;
	vec3 normal;
	vec2 tex_coords;
} vs_out;

void main()
{
	vec4 pos = u_model_view * vec4(a_position, 1);
	vs_out.position = vec3(pos) / pos.w;

	vs_out.normal = u_normal_mat * a_normal;
	vs_out.tex_coords = a_tex_coords;

	gl_Position = u_mvp * vec4(a_position, 1.);
}