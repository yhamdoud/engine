#version 460 core

in vec2 tex_coords;

uniform sampler2D u_g_position;
uniform sampler2D u_g_normal;
uniform sampler2D u_g_albedo_specular;
uniform sampler2D u_shadow_map;

struct Light {
    vec3 position;
    vec3 color;
};

const uint light_count = 3;
uniform Light u_lights[light_count];

out vec4 frag_color;

// Source: https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
float calculate_shadow(vec4 pos_light_space, vec3 normal, vec3 light_dir)
{
	vec3 pos = pos_light_space.xyz / pos_light_space.w;

	// Handle points beyond the far plane of the light's frustrum.
	if (pos.z > 1.0)
		return 0.;

	// Transform NDC to texture space.
	pos = 0.5 * pos + 0.5;

	float bias_max = 0.03;
	float bias_min = 0.001;

	float bias = max(bias_max * (1 - dot(normal, light_dir)), bias_min);

	// PCF
	vec2 scale = 1. / textureSize(u_shadow_map, 0);

	int range = 3;
	int sample_count = (2 * range + 1) * (2 * range + 1);

	float shadow = 0.;

	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			vec2 offset = vec2(x, y);
			float depth = texture(u_shadow_map, pos.xy + scale * offset).r;
			shadow += int(pos.z - bias > depth);
		}
	}

	return shadow / float(sample_count);
}

void main()
{
	// TODO: Differentiate between point and directional lights.
	vec4 albedo_specular = texture(u_g_albedo_specular, tex_coords);
	vec3 albedo = albedo_specular.rgb;
	float intensity = albedo_specular.a;

	vec3 pos = texture(u_g_position, tex_coords).xyz;
	vec3 n = texture(u_g_normal, tex_coords).xyz;
	vec3 v = normalize(-pos);

	float ambient = 0.1;
	vec3 col = ambient * albedo;

	for (uint i = 0; i < light_count; i++)
	{
	    Light light = u_lights[i];
        vec3 l = normalize(light.position - pos);
        vec3 h = normalize(l + v);

        float diffuse = max(dot(n, l), 0);
        float spec = pow(max(dot(n, h), 0), 16);

        col += diffuse * albedo * light.color + spec * light.color;
	}


// 	float shadow = calculate_shadow(fs_in.light_space_pos, n, l);
// 	diffuse *= (1-shadow);
// 	spec *= (1-shadow);

	frag_color = vec4(col, 1);
}