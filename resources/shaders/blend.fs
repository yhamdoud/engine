#version 460 core

in vec2 tex_coords;

uniform bool u_correct;

layout (binding = 0) uniform sampler2D u_g_normal_metallic;
layout (binding = 1) uniform sampler2D u_g_base_color_roughness;
layout (binding = 2) uniform sampler2D u_texture;

layout (location = 0) out vec4 frag_color;

vec3 F_Schlick_roughness(float u, vec3 f0, float roughness)
{
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - u, 0.0, 1.0), 5.0);
}   

float linearize_depth(float depth, mat4 proj)
{
    return -proj[3][2] / (2. * depth - 1. + proj[2][2]);
}

void main()
{

	float metallic = texture(u_g_normal_metallic, tex_coords).a;

	vec4 base_color_roughness = texture(u_g_base_color_roughness, tex_coords);
	vec3 base_color = base_color_roughness.rgb;
	float roughness = base_color_roughness.a;

	vec3 f0 = mix(vec3(0.04), base_color, metallic);

	vec4 p = textureLod(u_texture, tex_coords, roughness * 4);

	float n_dot_v = p.a;

    vec3 F = u_correct
		? F_Schlick_roughness(n_dot_v, f0, roughness)
		: vec3(1.);

	frag_color = textureLod(u_texture, tex_coords, 0).a * vec4(F * p.rgb, 1.);
}