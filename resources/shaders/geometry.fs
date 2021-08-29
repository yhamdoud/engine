
#version 460 core

layout (location = 0) out vec4 g_position;
layout (location = 1) out vec4 g_normal;
layout (location = 2) out vec4 g_albedo_specular;

uniform vec3 u_light_pos;
uniform mat4 u_model_view;
uniform bool u_use_sampler;

uniform sampler2D u_base_color;
uniform sampler2D u_shadow_map;

uniform float u_far_clip_distance;

in Varying
{
	// View space vectors.
	vec3 position;
	vec3 normal;
	vec2 tex_coords;
	vec4 light_space_pos;
} fs_in;

void main()
{
    float depth = -fs_in.position.z / u_far_clip_distance;
	gl_FragDepth = depth;
	g_normal = vec4(normalize(fs_in.normal), 1.);


	if (u_use_sampler)
        g_albedo_specular.rgb = texture(u_base_color, fs_in.tex_coords).rgb;
    else
        g_albedo_specular.rgb = vec3(0.5);

	g_albedo_specular.a = 0.3;
}