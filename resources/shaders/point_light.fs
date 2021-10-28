#version 460 core

#ifdef VALIDATOR
#extension GL_GOOGLE_include_directive : require
#endif

#include "/include/common.h"
#include "/include/pbs.h"
#include "/include/uniforms.h"

struct PointLight
{
    vec3 position;
    vec3 color;
    float radius;
    float radius_squared;
};

// TODO:
uniform mat4 u_view;

uniform PointLight u_light;

layout(std140, binding = 0) uniform Uniform { LightingUniforms u; };

layout(binding = 0) uniform sampler2D u_g_depth;
layout(binding = 1) uniform sampler2D u_g_normal_metallic;
layout(binding = 2) uniform sampler2D u_g_base_color_roughness;

out vec4 frag_color;

float attenuate(float dist_squared) { return 4. * PI / dist_squared; }

void main()
{
    vec2 tex_coords =
        gl_FragCoord.xy / textureSize(u_g_base_color_roughness, 0);

    vec3 light_pos = (u_view * vec4(u_light.position, 1.)).xyz;
    vec3 pos = pos_from_depth(
        linearize_depth(texture(u_g_depth, tex_coords).r, u.proj), tex_coords,
        u.proj_inv);

    vec3 pos_to_light = light_pos - pos;
    float dist_squared = dot(pos_to_light, pos_to_light);

    if (dist_squared > u_light.radius_squared)
        discard;

    vec3 l = normalize(pos_to_light);
    vec3 v = normalize(-pos);

    vec3 base_color;
    float roughness;
    vec3 n;
    float metallic;

    decode_g_buf(tex_coords, u_g_normal_metallic, u_g_base_color_roughness, n,
                 metallic, base_color, roughness);

    if (!u.base_color)
        base_color = vec3(1.);

    vec3 diffuse_color;
    vec3 f0;
    decode_material(base_color, metallic, diffuse_color, f0);

    vec3 luminance = attenuate(dist_squared) *
                     brdf(v, l, n, diffuse_color, f0, roughness) *
                     u_light.color;

    const float falloff_ratio = 0.7;
    luminance *=
        smoothstep(u_light.radius_squared,
                   u_light.radius_squared * falloff_ratio, dist_squared);

    frag_color = vec4(luminance, 1.);
}