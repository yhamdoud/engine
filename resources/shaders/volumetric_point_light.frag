#version 460 core

#ifdef VALIDATOR
#extension GL_GOOGLE_include_directive : require
#endif

#include "/include/common.h"
#include "/include/math.h"
#include "/include/uniforms.h"

struct PointLight
{
    vec3 position;
    vec3 color;
    float radius;
    float radius_squared;
};

uniform int u_light_idx;

uniform PointLight u_light;

layout(std140, binding = 0) uniform Uniform { VolumetricUniforms u; };

layout(binding = 1) uniform sampler2D u_g_depth;

layout(binding = 5) uniform samplerCubeArrayShadow u_shadow;

out vec4 frag_color;

float calculate_shadow(vec3 frag_pos_world, vec3 light_pos_world)
{
    vec3 light_to_frag = frag_pos_world - light_pos_world;
    float light_dist = length(light_to_frag);

    float shadow = 1. - texture(u_shadow, vec4(light_to_frag, u_light_idx),
                                light_dist / u_light.radius);

    return shadow;
}

float attenuate(float dist_squared) { return 4. * PI / dist_squared; }

bool ray_sphere_intersect(vec3 o, vec3 d, vec3 c, float r2, out vec2 hit)
{
    vec3 co = o - c;

    float b = dot(d, co);

    float dis = b * b - (dot(co, co) - r2);

    if (dis < 0.)
        return false;

    float x = sqrt(dis);

    hit.x = -b - x;
    hit.y = -b + x;

    return true;
}

void main()
{
    vec2 tex_coords = gl_FragCoord.xy / (u.read_size / 2);

    vec3 light_pos = (u.view * vec4(u_light.position, 1.)).xyz;
    float depth =
        linearize_depth(textureLod(u_g_depth, tex_coords, 1).r, u.proj);
    vec3 pos = pos_from_depth(depth, tex_coords, u.proj_inv);

    vec3 v = normalize(pos);

    // Raymarching
    vec2 hit;
    vec3 color;

    if (ray_sphere_intersect(vec3(0), v, light_pos, u_light.radius_squared,
                             hit))
    {
        // vec3 ray_origin = vec3(0.);
        vec3 ray_origin = max(0, hit.x) * v;

        // vec3 ray_end = pos;
        vec3 ray_end = min(-depth, hit.y) * v;

        vec3 ray = ray_end - ray_origin;
        float ray_len = length(ray);
        vec3 ray_dir = ray / ray_len;

        float step_size = ray_len / u.step_count;
        vec3 ray_step = step_size * ray_dir;

        vec3 fog = vec3(0.);

        ivec2 c = ivec2(gl_FragCoord.xy);
        float offset = bayer[c.x % 4][c.y % 4];
        vec3 ray_pos = ray_origin + offset * ray_step;

        for (int i = 0; i < u.step_count; i++)
        {
            vec3 ray_pos_world = (u.view_inv * vec4(ray_pos, 1.)).xyz;
            float shadow = calculate_shadow(ray_pos_world, u_light.position);

            vec3 pos_to_light = light_pos - ray_pos;
            float dist_squared = dot(pos_to_light, pos_to_light);

            vec3 l = normalize(pos_to_light);

            float factor =
                u.scatter_amount *
                henyey_greenstein(dot(ray_dir, l), u.scatter_intensity);

            fog +=
                attenuate(dist_squared) * u_light.color * factor * (1 - shadow);

            ray_pos += ray_step;
        }

        color = fog / u.step_count;
    }
    else
    {
        color = vec3(0., 0., 0.);
    }

    frag_color = vec4(color, 1.);
}