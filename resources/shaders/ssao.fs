#version 460 core

in vec2 tex_coords;
in vec3 view_ray;

layout (binding = 0) uniform sampler2D u_g_depth;
layout (binding = 1) uniform sampler2D u_g_normal_metallic;
layout (binding = 2) uniform sampler2D u_noise;

uniform mat4 u_proj;
uniform uint u_kernel_size;
uniform vec3 u_kernel[64];
uniform vec2 u_noise_scale;
uniform float u_radius;
uniform float u_bias;
uniform float u_strength;

layout (location = 0) out float frag_occlusion;
layout (location = 2) out vec4 debug;

void main()
{
    // View space position.
	vec3 pos = view_ray * texture(u_g_depth, tex_coords).r;
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

		// debug += vec4(sample_pos, 1) / 64.;

		// Project sample to screen (clip) space to get depth buffer coordinates.
		vec4 sample_clip = u_proj * vec4(sample_pos, 1.);
		// Map [-1, 1] to [0, 1].
		vec2 sample_coord = sample_clip.xy / sample_clip.w * 0.5 + 0.5;

		// Convert linear to view.
		float depth_at_sample = texture(u_g_depth, sample_coord.xy).x * -50.f;

		// Deals with samples that are not located near the original fragment,
		// e.g., samples near the edge.
		float range_check = smoothstep(0., 1., u_radius / abs(frag_depth - depth_at_sample));

		debug += vec4(sample_pos.z, depth_at_sample, frag_depth, range_check) / 64.;

		// Check if the sample is occluded by comparing it's depth with the depth
		// value sampled at it's position. Update the occlusion count accordingly.
		occlusion += (depth_at_sample >= sample_pos.z + u_bias ? 1. : 0.) * range_check;
	}

	frag_occlusion = pow(1.0 - occlusion / float(u_kernel_size), u_strength);
}
