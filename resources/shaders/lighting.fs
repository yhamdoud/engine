#version 460 core

#ifdef VALIDATOR
#extension GL_GOOGLE_include_directive : require
#define CASCADE_COUNT 1
#endif

#include "/include/common.h"
#include "/include/pbs.h"
#include "/include/shadow.h"
#include "/include/uniforms.h"

in vec2 tex_coords;
in vec3 view_ray;

layout(std140, binding = 0) uniform Uniform { LightingUniforms u; };

layout(binding = 0) uniform sampler2D u_g_depth;
layout(binding = 1) uniform sampler2D u_g_normal_metallic;
layout(binding = 2) uniform sampler2D u_g_base_color_roughness;
layout(binding = 3) uniform sampler2DArrayShadow u_shadow_map;
layout(binding = 4) uniform sampler2D u_ao;

layout(binding = 5) uniform sampler3D u_sh_0;
layout(binding = 6) uniform sampler3D u_sh_1;
layout(binding = 7) uniform sampler3D u_sh_2;
layout(binding = 8) uniform sampler3D u_sh_3;
layout(binding = 9) uniform sampler3D u_sh_4;
layout(binding = 10) uniform sampler3D u_sh_5;
layout(binding = 11) uniform sampler3D u_sh_6;

layout(binding = 12) uniform sampler2D u_hdr_prev;
layout(binding = 13) uniform sampler2D u_reflections;
layout(binding = 14) uniform sampler2D u_g_velocity;

uniform mat4 u_light_transforms[CASCADE_COUNT];
uniform float u_cascade_distances[CASCADE_COUNT];

out vec4 frag_color;

// Source: https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
float calculate_shadow(vec4 light_pos, uint cascade_idx, vec3 normal,
                       vec3 light_dir)
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
    if (u.filter_shadows)
    {
        int range = 3;
        int sample_count = (2 * range + 1) * (2 * range + 1);
        vec2 scale = 1. / textureSize(u_shadow_map, 0).xy;

        for (int x = -range; x <= range; x++)
        {
            for (int y = -range; y <= range; y++)
            {
                vec2 offset = vec2(x, y);
                shadow +=
                    1. - texture(u_shadow_map, vec4(pos.xy + scale * offset,
                                                    cascade_idx, pos.z + bias));
            }
        }

        shadow /= float(sample_count);
    }
    else
    {
        shadow +=
            1. - texture(u_shadow_map, vec4(pos.xy, cascade_idx, pos.z + bias));
    }

    return shadow;
}

vec3 calculate_indirect_lighting(vec3 pos, vec3 n)
{
    vec3 pos_world = (u.view_inv * vec4(pos, 1.f)).xyz;
    vec3 n_world = mat3(u.view_inv) * n;

    // Offset sample in the direction off the normal to reduce leaking.
    vec3 sample_pos = pos_world + u.leak_offset * n_world;
    // Transform world position to probe grid texture coordinates.
    vec3 grid_coords = vec3(u.inv_grid_transform * vec4(sample_pos, 1.f));
    vec3 grid_tex_coords = (grid_coords + vec3(0.5f)) / u.grid_dims;

    vec4 c0 = texture(u_sh_0, grid_tex_coords);
    vec4 c1 = texture(u_sh_1, grid_tex_coords);
    vec4 c2 = texture(u_sh_2, grid_tex_coords);
    vec4 c3 = texture(u_sh_3, grid_tex_coords);
    vec4 c4 = texture(u_sh_4, grid_tex_coords);
    vec4 c5 = texture(u_sh_5, grid_tex_coords);
    vec4 c6 = texture(u_sh_6, grid_tex_coords);
    vec3 c7 = vec3(c0.w, c1.w, c2.w);
    vec3 c8 = vec3(c3.w, c4.w, c5.w);

    // clang-format off
    vec3 irradiance =
        c0.rgb * 0.282095 +
        c1.rgb * 0.488603 * n_world.y +
        c2.rgb * 0.488603 * n_world.z +
        c3.rgb * 0.488603 * n_world.x +
        c4.rgb * 1.092548 * n_world.x * n_world.y +
        c5.rgb * 1.092548 * n_world.y * n_world.z +
        c6.rgb * 0.315392 * (3.f * n_world.z * n_world.z - 1.f) +
        c7.rgb * 1.092548 * n_world.x * n_world.z +
        c8.rgb * 0.546274 * (n_world.x * n_world.x - n_world.y * n_world.y);
    // clang-format on

    float occlusion = u.ambient_occlusion ? texture(u_ao, tex_coords).r : 1.;

    return occlusion * irradiance * Fd_Lambert();
}

void main()
{
    float depth;
    vec3 base_color;
    float roughness;
    vec3 n;
    float metallic;

    decode_g_buf(tex_coords, u_g_depth, u_g_normal_metallic,
                 u_g_base_color_roughness, depth, n, metallic, base_color,
                 roughness);

    vec3 diffuse_color;
    vec3 f0;

    decode_material(base_color, metallic, diffuse_color, f0);

    vec3 pos = view_ray * linearize_depth(depth, u.proj);
    vec3 v = normalize(-pos);

    vec3 out_luminance = vec3(0.);

    uint cascade_idx = calculate_cascade_index(pos, u_cascade_distances);

    // Direct lighting.
    if (u.direct_lighting)
    {
        // Directional light (sun) contribution.
        vec3 l = normalize(-u.light_direction);
        vec3 luminance =
            brdf(v, l, n, diffuse_color, f0, roughness) * u.light_intensity;

        // Fragment position in light space.
        vec4 light_pos =
            u_light_transforms[cascade_idx] * u.view_inv * vec4(pos, 1.);
        float shadow = calculate_shadow(light_pos, cascade_idx, n, l);

        out_luminance += (1. - shadow) * luminance;
    }

    if (u.indirect_lighting)
        out_luminance += base_color * calculate_indirect_lighting(pos, n);

    // SSR
    if (u.reflections)
    {
        vec4 r = textureLod(u_reflections, tex_coords, 0);

        float n_dot_v = r.a;

        vec3 F = F_Schlick_roughness(n_dot_v, f0, roughness);
        int max_lod = 4;

        // Reprojection.
        vec2 velocity = texture(u_g_velocity, tex_coords).rg;
        out_luminance +=
            r.b * F *
            textureLod(u_hdr_prev, r.rg - velocity, roughness * max_lod).rgb;
    }

    // Give different shadow map cascades a recognizable tint.
    if (u.color_cascades)
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