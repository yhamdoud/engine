#version 460 core

layout(location = 0) in vec3 a_pos;

uniform mat4 u_model;
uniform mat4 u_view_proj;

out vec3 v_pos;

void main()
{
    vec4 pos = u_model * vec4(a_pos, 1);
    v_pos = pos.xyz;
    gl_Position = u_view_proj * pos;
}