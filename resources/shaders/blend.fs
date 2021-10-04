#version 460 core

in vec2 tex_coords;

layout (binding = 0) uniform sampler2D u_g_base_color_roughness;
layout (binding = 1) uniform sampler2D u_texture;

layout (location = 0) out vec4 frag_color;

void main()
{
	float roughness = texture(u_g_base_color_roughness, tex_coords).a;
	frag_color = textureLod(u_texture, tex_coords, roughness * 5);
}