#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat4 u_model_view;

uniform float u_far_clip_distance;

out vec3 normal;
out vec3 position;

void main()
{
    position = (u_model * vec4(a_position, 1.)).xyz;
	normal = a_normal;
	gl_Position = u_mvp * vec4(a_position, 1.);
}
