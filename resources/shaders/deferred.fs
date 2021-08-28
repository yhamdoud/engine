
#version 460 core

layout (location = 0) out vec3 g_position;
layout (location = 1) out vec3 g_normal;
layout (location = 2) out vec4 g_albedo_specular;

uniform vec3 u_light_pos;
uniform mat4 u_model_view;
uniform bool u_use_sampler;

uniform sampler2D u_base_color;
uniform sampler2D u_shadow_map;

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
	// TODO: Differentiate between point and directional lights.
	// vec3 l = normalize(u_light_pos - fs_in.position);
	// vec3 v = normalize(-fs_in.position);
	// vec3 n = normalize(fs_in.normal);
	// vec3 h = normalize(l + v);

	// float ambient = 0.3;
	// float diffuse = max(dot(n, l), 0);
	// float spec = pow(max(dot(n, h), 0), 16);

	// vec3 ambient_color = vec3(1, 1, 0);
	// vec3 diffuse_color = vec3(1, 1, 0);
	// vec3 light_color = vec3(1);

	// float shadow = calculate_shadow(fs_in.light_space_pos, n, l);
	// diffuse *= (1-shadow);
	// spec *= (1-shadow);

	// vec3 col = ambient * ambient_color + diffuse * diffuse_color + spec * light_color;

	// frag_color = vec4(col, 1);
	g_position = fs_in.position;
	g_normal = normalize(fs_in.normal);

	if (u_use_sampler)
        g_albedo_specular.rgb = texture(u_base_color, fs_in.tex_coords).rgb;
    else
        g_albedo_specular.rgb = vec3(0.5);

	g_albedo_specular.a = 0.3;
}