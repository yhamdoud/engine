#version 460 core

layout(location = 0) in vec3 a_pos;

out vec3 tex_coords;

uniform mat4 u_projection;
uniform mat4 u_view;

void main()
{
    tex_coords = a_pos;
    vec4 pos = u_projection * u_view * vec4(a_pos, 1);
    // Draw skybox behind other fragments without disabling depth testing.
    gl_Position = pos.xyww;
}