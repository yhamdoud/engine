#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;

uniform mat4 u_mvp;
uniform mat4 u_model_view;

out vec3 tex_coords;
out vec3 position;

void main()
{
    position = (u_model_view * vec4(a_position, 1.)).xyz;
	tex_coords = a_normal;
	gl_Position = u_mvp * vec4(a_position, 1.);
}
