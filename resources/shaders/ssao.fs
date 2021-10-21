#version 460 core

#ifdef VALIDATOR
    #extension GL_GOOGLE_include_directive : require
#endif

#include "/include/common.h"

in vec2 tex_coords;
in vec3 view_ray;

layout (binding = 0) uniform sampler2D u_g_depth;
layout (binding = 1) uniform sampler2D u_g_normal_metallic;
layout (binding = 2) uniform sampler2D u_noise;

layout (location = 0) out float frag_occlusion;

layout (std140, binding = 3) uniform Uniforms
{
    int u_kernel_size;
    int u_sample_count;
    float u_radius;
    float u_bias;
    mat4 u_proj;
    vec2 u_noise_scale;
    float u_strength;
};

uniform vec3 u_kernel[64];

void main()
{
    // View space position.
    vec3 pos = view_ray * linearize_depth(texture(u_g_depth, tex_coords).r, u_proj);
    vec3 normal = texture(u_g_normal_metallic, tex_coords).xyz;
    vec3 random = texture(u_noise, tex_coords * u_noise_scale).xyz;

    // Use Gramm-Schimdt process to compute and orthogonal basis.
    vec3 tangent = normalize(random - normal * dot(random, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    const float frag_depth = pos.z;

    // https://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html
    float occlusion = 0.;
    for(uint i = 0; i < u_kernel_size; i++)
    {
        // Tangent to view space.
        vec3 offset = tbn * u_kernel[i];
        vec3 sample_pos = pos + u_radius * offset;

        // Project sample to screen (clip) space to get depth buffer coordinates.
        vec4 sample_clip = u_proj * vec4(sample_pos, 1.);
        // Map [-1, 1] to [0, 1].
        vec2 sample_coord = sample_clip.xy / sample_clip.w * 0.5 + 0.5;

        // Convert linear to view.
        float depth_at_sample = linearize_depth(texture(u_g_depth, sample_coord.xy).r, u_proj);

        // Deals with samples that are not located near the original fragment,
        // e.g., samples near the edge.
        float range_check = smoothstep(0., 1., u_radius / abs(frag_depth - depth_at_sample));

        // Check if the sample is occluded by comparing it's depth with the depth
        // value sampled at it's position. Update the occlusion count accordingly.
        occlusion += (depth_at_sample >= sample_pos.z + u_bias ? 1. : 0.) * range_check;
    }

    frag_occlusion = pow(1.0 - occlusion / float(u_kernel_size), u_strength);
}
