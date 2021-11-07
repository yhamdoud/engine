#ifndef COMMON_H
#define COMMON_H

// https://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer/
float linearize_depth(float depth, mat4 proj)
{
    return -proj[3][2] / (2. * depth - 1. + proj[2][2]);
}

void swap(inout float a, inout float b)
{
    float t = a;
    a = b;
    b = t;
}

float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

float luma(vec4 c) { return luma(vec3(c)); }

float saturate(float x) { return clamp(x, 0., 1.); }

vec2 saturate(vec2 x) { return clamp(x, 0., 1.); }

vec3 saturate(vec3 x) { return clamp(x, 0., 1.); }

float distance_squared(vec2 a, vec2 b)
{
    a -= b;
    return dot(a, a);
}

vec2 calc_tex_coords(ivec2 index, ivec2 size)
{
    return (vec2(index) + 0.5) / vec2(size);
}

vec2 calc_tex_coords(uvec2 index, uvec2 size)
{
    return (vec2(index) + 0.5) / vec2(size);
}

vec3 pos_from_depth(float depth, vec2 tex_coords, mat4 proj_inv)
{
    vec4 pos_ndc = vec4(tex_coords * 2. - 1., 0., 1.);
    vec4 pos_view = proj_inv * pos_ndc;

    return vec3(pos_view.xy / pos_view.z, 1.) * depth;
}

void decode_g_buf(vec2 tex_coords, sampler2D normal_metallic,
                  sampler2D base_color_roughness, out vec3 normal,
                  out float metallic, out vec3 base_color, out float roughness)
{
    vec4 n_m = texture(normal_metallic, tex_coords);
    normal = n_m.rgb;
    metallic = n_m.a;

    vec4 bc_r = texture(base_color_roughness, tex_coords);
    base_color = bc_r.rgb;
    roughness = bc_r.a;
}

void decode_g_buf(vec2 tex_coords, sampler2D depth_tex,
                  sampler2D normal_metallic, sampler2D base_color_roughness,
                  out float depth, out vec3 normal, out float metallic,
                  out vec3 base_color, out float roughness)
{
    depth = texture(depth_tex, tex_coords).r;

    decode_g_buf(tex_coords, normal_metallic, base_color_roughness, normal,
                 metallic, base_color, roughness);
}

void decode_material(vec3 base_color, float metallic, out vec3 diffuse_color,
                     out vec3 f0)
{
    // Non-metals have achromatic specular reflectance, metals use base color as
    // the specular color.
    diffuse_color = (1.0 - metallic) * base_color.rgb;
    f0 = mix(vec3(0.04), base_color, metallic);
}

#endif