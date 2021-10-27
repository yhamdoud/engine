#version 460 core

in vec2 tex_coords;

uniform sampler2D u_read;

void main()
{
	gl_FragDepth = textureLod(u_read, tex_coords, 0.).r;
}