#version 460 core

#ifdef VALIDATOR
    #extension GL_GOOGLE_include_directive : require
    #define CASCADE_COUNT 3
#endif

#include "/include/common.h"
#include "/include/pbs.h"
#include "/include/shadow.h"

in vec2 tex_coords;
in vec3 view_ray;

uniform mat4 u_proj;

layout (binding = 0) uniform sampler2D u_g_depth;
layout (binding = 1) uniform sampler2D u_g_normal_metallic;
layout (binding = 2) uniform sampler2D u_g_base_color_roughness;
layout (binding = 14) uniform sampler2D u_g_velocity;

layout (binding = 3) uniform sampler2DArrayShadow u_shadow_map;

layout (binding = 4) uniform sampler2D u_ao;

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

uniform mat4 u_light_transforms[CASCADE_COUNT];

uniform float u_cascade_distances[CASCADE_COUNT];

uniform mat4 u_view_inv;
uniform bool u_color_cascades;
uniform bool u_filter;

// Diffuse GI
uniform mat4 u_inv_grid_transform;
uniform vec3 u_grid_dims;
uniform float u_leak_offset;

uniform bool u_ssr;
uniform bool u_ssao;

layout (binding = 5) uniform sampler3D u_sh_0;
layout (binding = 6) uniform sampler3D u_sh_1;
layout (binding = 7) uniform sampler3D u_sh_2;
layout (binding = 8) uniform sampler3D u_sh_3;
layout (binding = 9) uniform sampler3D u_sh_4;
layout (binding = 10) uniform sampler3D u_sh_5;
layout (binding = 11) uniform sampler3D u_sh_6;

layout (binding = 12) uniform sampler2D u_hdr_prev;
layout (binding = 13) uniform sampler2D u_reflections;

out vec4 frag_color;

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

    float shadow = 0.;

    // PCF
    if (u_filter)
    {
        int range = 3;
        int sample_count = (2 * range + 1) * (2 * range + 1);
        vec2 scale = 1. / textureSize(u_shadow_map, 0).xy;

        for (int x = -range; x <= range; x++)
        {
            for (int y = -range; y <= range; y++)
            {
                vec2 offset = vec2(x, y);
                shadow += 1. - texture(
                    u_shadow_map,
                    vec4(pos.xy + scale * offset, cascade_idx, pos.z + bias)
                );
            }
        }

        shadow /= float(sample_count);
    }
    else 
    {
        shadow += 1. - texture(u_shadow_map, vec4(pos.xy, cascade_idx, pos.z + bias));
    }

    return shadow;
}


vec3 brdf(vec3 v, vec3 l, vec3 n, vec3 diffuse_color, vec3 f0, float roughness)
{
    const vec3 h = normalize(v + l);

    const float n_dot_v = abs(dot(n, v)) + 1e-5;
    const float n_dot_l = clamp(dot(n, l), 0., 1.);
    const float n_dot_h = clamp(dot(n, h), 0., 1.);
    const float l_dot_h = clamp(dot(l, h), 0., 1.);

    const float a = roughness * roughness;
    // Clamp roughness to prevent NaN fragments.
    const float a2 = max(0.01, a * a);

    const float D = D_GGX(n_dot_h, a2);
    const vec3 F = F_Schlick(l_dot_h, f0);
    const float V = V_SmithGGXCorrelated(n_dot_v, n_dot_l, a2);

    // Specular BRDF component.
    // V already contains the denominator from Cook-Torrance.
    vec3 Fr = D * V * F;

    // Diffuse BRDF component.
    vec3 Fd = diffuse_color * Fd_Lambert();
    vec3 Fin = F_Schlick(n_dot_l, f0);
    vec3 Fout = F_Schlick(n_dot_v, f0);

    // TODO: Energy conserving but causes artifacts at edges.
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#coupling-diffuse-and-specular-reflection
    // https://computergraphics.stackexchange.com/questions/2285/how-to-properly-combine-the-diffuse-and-specular-terms
    // return ((1 - Fin) * (1 - Fout) * Fd + Fr) * NoL;

    return (Fd + Fr) * n_dot_l;
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
    vec3 n = normal_metallic.rgb;
    float metallic = normal_metallic.a;

    // View space position.
    vec3 pos = view_ray * linearize_depth(texture(u_g_depth, tex_coords).r, u_proj);
    vec3 v = normalize(-pos);

    // Non-metals have achromatic specular reflectance, metals use base color
    // as the specular color.
    vec3 diffuse_color = (1.0 - metallic) * base_color.rgb;

    // TODO: More physically accurate way to calculate?
    vec3 f0 = mix(vec3(0.04), base_color, metallic);

    // Luminance or radiance?
    vec3 out_luminance = vec3(0.);

    uint cascade_idx = calculate_cascade_index(pos, u_cascade_distances);

    // Direct lighting.
    if (u_use_direct) {
        // Directional light (sun) contribution.
        vec3 l = normalize(-u_directional_light.direction);
        vec3 luminance = brdf(v, l, n, diffuse_color, f0, roughness) * u_directional_light.intensity;

        // Point lights contribution.
        for (uint i = 0; i < light_count; i++)
        {
            Light light = u_lights[i];
            float dist = length(light.position - pos);
            // Inverse square law
            // TODO: Might not be a good fit, can cause divide by zero.
            float attenuation = 1 / (dist * dist);

            l = normalize(light.position - pos);

            out_luminance += brdf(v, l, n, diffuse_color, f0, roughness) * light.color * attenuation;
        }

        // Fragment position in light space.

        vec4 light_pos = u_light_transforms[cascade_idx] * u_view_inv * vec4(pos, 1.);
        float shadow = calculate_shadow(light_pos, cascade_idx, n, l);

        out_luminance += (1. - shadow) * luminance;
    }

    // Indirect lighting.
    if (u_use_irradiance)
    {
        vec3 pos_world = (u_view_inv * vec4(pos, 1.f)).xyz;
        vec3 n_world = mat3(u_view_inv) * n;

        // Offset sample in the direction off the normal to reduce leaking.
        vec3 sample_pos = pos_world + u_leak_offset * n_world;
        // Transform world position to probe grid texture coordinates.
        vec3 grid_coords = vec3(u_inv_grid_transform * vec4(sample_pos, 1.f));
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
                          c1.rgb * 0.488603f * n_world.y +
                          c2.rgb * 0.488603f * n_world.z +
                          c3.rgb * 0.488603f * n_world.x +
                          c4.rgb * 1.092548f * n_world.x * n_world.y +
                          c5.rgb * 1.092548f * n_world.y * n_world.z +
                          c6.rgb * 0.315392f * (3.f * n_world.z * n_world.z - 1.f) +
                          c7.rgb * 1.092548f * n_world.x * n_world.z +
                          c8.rgb * 0.546274f * (n_world.x * n_world.x - n_world.y * n_world.y);

        float occlusion = u_ssao ? texture(u_ao, tex_coords).r : 1.;

        out_luminance += occlusion * irradiance * base_color * Fd_Lambert();
    }

    // SSR
    if (u_ssr)
    {
        vec4 r = textureLod(u_reflections, tex_coords, 0);

        float n_dot_v = r.a;

        vec3 F = F_Schlick_roughness(n_dot_v, f0, roughness);
        int max_lod = 4;

        // Reprojection.
        vec2 velocity = texture(u_g_velocity, tex_coords).rg;
        out_luminance +=
            r.b * F * textureLod(
                u_hdr_prev,
                // r.rg - u_dt * velocity,
                r.rg,
                roughness * max_lod
            ).rgb;
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