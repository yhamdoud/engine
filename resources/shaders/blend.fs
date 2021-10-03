#version 460 core

in vec2 tex_coords;

layout (location = 0) uniform sampler2D u_texture;

layout (location = 0) out vec4 frag_color;

void main()
{
	frag_color = texture(u_texture, tex_coords);
}