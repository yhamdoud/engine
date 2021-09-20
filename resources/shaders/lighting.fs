#version 460 core

in vec2 tex_coords;
in vec3 view_ray;


uniform sampler2D u_g_depth;
uniform sampler2D u_g_normal_metallic;
uniform sampler2D u_g_base_color_roughness;
uniform sampler2DArray u_shadow_map;
uniform sampler2D u_ao;

uniform bool u_use_irradiance;
uniform bool u_use_direct;
uniform bool u_use_base_color;

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

const uint cascade_count = 3;
uniform mat4 u_light_transforms[cascade_count];
uniform float u_cascade_distances[cascade_count];
uniform mat4 u_view_inv;
uniform bool u_color_cascades;

// Diffuse GI
uniform mat4 u_inv_grid_transform;
uniform vec3 u_grid_dims;

uniform sampler3D u_sh_0;
uniform sampler3D u_sh_1;
uniform sampler3D u_sh_2;
uniform sampler3D u_sh_3;
uniform sampler3D u_sh_4;
uniform sampler3D u_sh_5;
uniform sampler3D u_sh_6;

out vec4 frag_color;

const float PI = 3.14159265359;

uint calculate_cascade_index(vec3 pos)
{
    uint cascade_idx = cascade_count - 1;
    for (int i = 0; i < cascade_count; i++)
    {
        // Sign change
        if (-pos.z < u_cascade_distances[i])
        {
            cascade_idx = i;
            break;
        }
    }

    return cascade_idx;
}


// Source: https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
float calculate_shadow(vec4 light_pos, uint cascade_idx, vec3 normal, vec3 light_dir)
{

    vec3 pos = light_pos.xyz / light_pos.w;

	// Handle points beyond the far plane of the light's frustrum.
	if (pos.z > 1.0)
		return 0.;

	// Transform NDC to texture space.
	pos = 0.5 * pos + 0.5;

	float bias_max = 0.03;
	float bias_min = 0.001;

	float bias = max(bias_max * (1 - dot(normal, light_dir)), bias_min);
    // FIXME: temp
    bias = 0;

	// PCF
	vec2 scale = 1. / textureSize(u_shadow_map, 0).xy;

	int range = 3;
	int sample_count = (2 * range + 1) * (2 * range + 1);

	float shadow = 0.;

	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			vec2 offset = vec2(x, y);
			float depth = texture(u_shadow_map, vec3(pos.xy + scale * offset, cascade_idx)).r;
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

    vec3 base_color;
    if (u_use_base_color)
        base_color = base_color_roughness.rgb;
    else
        base_color = vec3(1.f);

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


    uint cascade_idx = calculate_cascade_index(pos);

    // Direct lighting.
    if (u_use_direct) {
        // Point lights contribution.
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

        // Directional light (sun) contribution.
        vec3 l = -u_directional_light.direction;
        vec3 luminance = brdf(v, l) * u_directional_light.intensity;

        // Fragment position in light space.

        vec4 light_pos = u_light_transforms[cascade_idx] * u_view_inv * vec4(pos, 1.);
        float shadow = calculate_shadow(light_pos, cascade_idx, n, l);

        out_luminance += (1. - shadow) * luminance;
    }

    // Indirect lighting.
    if (u_use_irradiance)
    {
        vec3 world_pos = (u_view_inv * vec4(pos, 1.f)).xyz;
        vec3 N = mat3(u_view_inv) * n;

        // Transform world position to probe grid texture coordinates.
        vec3 grid_coords = vec3(u_inv_grid_transform * vec4(world_pos, 1.f));
        vec3 grid_tex_coords = (grid_coords + vec3(0.5f)) / u_grid_dims;

        vec4 c0 = texture(u_sh_0, grid_tex_coords);
        vec4 c1 = texture(u_sh_1, grid_tex_coords);
        vec4 c2 = texture(u_sh_2, grid_tex_coords);
        vec4 c3 = texture(u_sh_3, grid_tex_coords);
        vec4 c4 = texture(u_sh_4, grid_tex_coords);
        vec4 c5 = texture(u_sh_5, grid_tex_coords);
        vec4 c6 = texture(u_sh_6, grid_tex_coords);
        vec3 c7 = vec3(c0.w, c1.w, c2.w);
        vec3 c8 = vec3(c3.w, c4.w, c5.w);

        vec3 irradiance = c0.rgb * 0.282095f +
                          c1.rgb * 0.488603f * N.y +
                          c2.rgb * 0.488603f * N.z +
                          c3.rgb * 0.488603f * N.x +
                          c4.rgb * 1.092548f * N.x * N.y +
                          c5.rgb * 1.092548f * N.y * N.z +
                          c6.rgb * 0.315392f * (3.f * N.z * N.z - 1.f) +
                          c7.rgb * 1.092548f * N.x * N.z +
                          c8.rgb * 0.546274f * (N.x * N.x - N.y * N.y);

        vec3 kS = F_Schlick(max(dot(n, v), 0.), f0);
        // TODO: This isn't correct, I think.
        vec3 kD = 1.0 - kS;

        float occlusion = texture(u_ao, tex_coords).r;

        out_luminance += occlusion * irradiance * base_color * Fd_Lambert();
	}

    // Give different shadow map cascades a recognizable tint.
    if (u_color_cascades)
    {
        switch (cascade_idx)
        {
        case 0:
            out_luminance *= vec3(1.0, 0.2, 0.2);
            break;
        case 1:
            out_luminance *= vec3(0.2, 1.0, 0.2);
            break;
        case 2:
            out_luminance *= vec3(0.2, 0.2, 1.0);
            break;
        }
    }

	frag_color = vec4(out_luminance, 1.);
}