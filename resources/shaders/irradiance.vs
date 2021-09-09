#version 460 core
layout (location = 0) in vec3 a_position;

out vec3 world_position;

uniform mat4 u_projection;
uniform mat4 u_view;

void main()
{
    world_position = a_position;
    gl_Position =  u_projection * u_view * vec4(world_position, 1.0);
}

