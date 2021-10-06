#version 460 core

// TODO: Use this when when using hardware depth buffer.
// layout(early_fragment_tests) in;

layout (location = 0) out vec4 g_normal_metallic;
layout (location = 1) out vec4 g_base_color_roughness;
layout (location = 2) out vec4 g_velocity;

uniform vec3 u_light_pos;
uniform mat3 u_normal_mat;

uniform bool u_use_sampler;
uniform bool u_use_normal;
uniform bool u_use_metallic_roughness;

uniform sampler2D u_base_color;
uniform sampler2D u_metallic_roughness;
uniform sampler2D u_normal;
uniform sampler2D u_shadow_map;

uniform float u_metallic_factor;
uniform float u_roughness_factor;
uniform vec3 u_base_color_factor;

uniform float u_far_clip_distance;

uniform bool u_alpha_mask;
uniform float u_alpha_cutoff;

in Varying
{
    // View space vectors.
    vec3 normal;
    vec2 tex_coords;
    vec4 tangent;
    vec4 light_space_pos;
    vec4 position;
    vec4 position_prev;
} fs_in;

mat3 calculate_tbn_matrix(vec4 tangent_sign, vec3 normal)
{
    normal = normalize(normal);
    vec3 tangent = normalize(tangent_sign.xyz);
    float bitangent_sign = tangent_sign.w;

    vec3 bitangent = cross(normal, tangent) * bitangent_sign;

    return mat3(tangent, bitangent, normal);
}

float saturate(float x)
{
    return clamp(x, 0., 1.);
}

void main()
{
    g_base_color_roughness.rgb = u_base_color_factor;

    if (u_use_sampler)
    {
        vec4 base_color_alpha = texture(u_base_color, fs_in.tex_coords);
        if (u_alpha_mask && base_color_alpha.a < u_alpha_cutoff)
        {
            discard; // FIXME: Bad.
        }

        g_base_color_roughness.rgb *= base_color_alpha.rgb;
    }

    if (u_use_normal)
    {
        mat3 tbn = calculate_tbn_matrix(fs_in.tangent, fs_in.normal);
        vec3 normal_tangent = vec3(texture(u_normal, fs_in.tex_coords).xy * 2. -1., 0);
        // Reconstruct z-component of normal.
        // TODO: Maybe disable this when loading uncompressed textures.
        normal_tangent.z = sqrt(saturate(1. - dot(normal_tangent.x, normal_tangent.z)));

        g_normal_metallic.xyz = tbn * normal_tangent;
    }
    else
    {
        g_normal_metallic = vec4(normalize(fs_in.normal), 1.);
    }

    g_normal_metallic.a = u_metallic_factor;
    g_base_color_roughness.a = u_roughness_factor;

    if (u_use_metallic_roughness)
    {
        vec2 metallic_roughness = texture(u_metallic_roughness, fs_in.tex_coords).bg;

        g_normal_metallic.a *= metallic_roughness[0];
        g_base_color_roughness.a *= metallic_roughness[1];
    }

    vec3 pos = fs_in.position.xyz / fs_in.position.w;
    vec3 pos_prev = fs_in.position_prev.xyz / fs_in.position_prev.w;

    vec2 velocity = pos.xy - pos_prev.xy;
    g_velocity = vec4(velocity, 0., 1.);
}