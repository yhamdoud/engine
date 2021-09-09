#version 460 core

in vec2 tex_coords;
in vec3 view_ray;

uniform sampler2D u_g_depth;
uniform sampler2D u_g_normal_metallic;
uniform sampler2D u_g_base_color_roughness;
uniform sampler2D u_shadow_map;

uniform bool u_use_irradiance;
uniform samplerCube u_irradiance;

struct Light {
    vec3 position;
    vec3 color;
};

struct DirectionalLight {
    vec3 intensity;
    vec3 direction;
};

uniform DirectionalLight u_directional_light;

const uint light_count = 3;
uniform Light u_lights[light_count];

uniform mat4 u_light_transform;
uniform mat4 u_view_inv;

out vec4 frag_color;

const float PI = 3.14159265359;

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

// PBS

// Normal distribution function
// Source: https://google.github.io/filament
float D_GGX(float NoH, float roughness)
{
    float a = NoH * roughness;
    float k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

// Geometric shadowing
// Source: https://google.github.io/filament
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness)
{
    float a2 = roughness * roughness;
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

// Fresnel
// Source: https://google.github.io/filament
vec3 F_Schlick(float u, vec3 f0)
{
    float f = pow(1.0 - u, 5.0);
    return f + f0 * (1.0 - f);
}

// Source: https://google.github.io/filament
float Fd_Lambert() {
    return 1.0 / PI;
}

vec3 n;
vec3 diffuse_color;
vec3 f0;
float a;

vec3 brdf(vec3 v, vec3 l)
{
	vec3 h = normalize(v + l);

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    float D = D_GGX(NoH, a);
    vec3  F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, a);

    // Specular BRDF component.
    // V already contains the denominator from Cook-Torrance.
    vec3 Fr = D * V * F;

    // Diffuse BRDF component.
    vec3 Fd = diffuse_color * Fd_Lambert();
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#coupling-diffuse-and-specular-reflection
    // https://computergraphics.stackexchange.com/questions/2285/how-to-properly-combine-the-diffuse-and-specular-terms
    vec3 Fin = F_Schlick(NoL, f0);
    vec3 Fout = F_Schlick(NoV, f0);

    // TODO: Causes black artifacts, weighing the diffuse component causes black artifacts.
    //	out_radiance += ((1 - Fin) * (1 - Fout) * Fd + Fr) * light_radiance * NoL;
    return (Fd + Fr) * NoL;
}

void main()
{
	// TODO: Differentiate between point and directional point_lights.
	vec4 base_color_roughness = texture(u_g_base_color_roughness, tex_coords);
	vec3 base_color = base_color_roughness.rgb;
	float roughness = base_color_roughness.a;


	vec4 normal_metallic = texture(u_g_normal_metallic, tex_coords);
	n = normal_metallic.rgb;
	float metallic = normal_metallic.a;

    // View space position.
	vec3 pos = view_ray * texture(u_g_depth, tex_coords).x;
	vec3 v = normalize(-pos);

	// Non-metals have achromatic specular reflectance, metals use base color
	// as the specular color.
	diffuse_color = (1.0 - metallic) * base_color.rgb;

	// Perceptually linear roughness to roughness (alpha).
	// Clamp to avoid specular aliasing.
	a = clamp(roughness * roughness, 0.002, 1.);

	// TODO: More physically accurate way to calculate?
	f0 = mix(vec3(0.04), base_color, metallic);

	// Luminance or radiance?
	vec3 out_luminance = vec3(0.);

	for (uint i = 0; i < light_count; i++)
	{
	    Light light = u_lights[i];
		float dist = length(light.position - pos);
		// Inverse square law
		// TODO: Might not be a good fit, can cause divide by zero.
		float attenuation = 1 / (dist * dist);

        vec3 l = normalize(light.position - pos);

        out_luminance += brdf(v, l) * light.color * attenuation;
	}

    if (u_use_irradiance)
    {
        vec3 kS = F_Schlick(max(dot(n, v), 0.), f0);
        vec3 kD = 1.0 - kS;
        // TODO
        vec3 irradiance = texture(u_irradiance, mat3(u_view_inv) * n).rgb;
        vec3 diffuse = irradiance * base_color;
        vec3 ambient = (kD * diffuse);
        out_luminance += ambient;
	}

    // Directional light (sun).
    vec3 l = -u_directional_light.direction;
	vec3 luminance = brdf(v, l) * u_directional_light.intensity;
	float shadow = calculate_shadow(u_light_transform * u_view_inv * vec4(pos, 1.), n, l);

	out_luminance += (1. - shadow) * luminance;

    // TODO: Gamma correction, need to this investigate further.
    // vec3 color = pow(out_luminance / (out_luminance + vec3(1.)), vec3(1. / 2.2));

	frag_color = vec4(out_luminance, 1.);
}