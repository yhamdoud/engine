#version 460 core

in vec3 tex_coords;

out vec4 frag_color;

uniform samplerCube u_skybox;

void main()
{
    frag_color = texture(u_skybox, tex_coords);
}