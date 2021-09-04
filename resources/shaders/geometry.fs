
#version 460 core

layout (location = 0) out vec4 g_position;
layout (location = 1) out vec4 g_normal_metallic;
layout (location = 2) out vec4 g_base_color_roughness;

uniform vec3 u_light_pos;
uniform mat4 u_model_view;
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

uniform float u_far_clip_distance;

in Varying
{
	// View space vectors.
	vec3 position;
	vec3 normal;
	vec2 tex_coords;
	vec4 tangent;
	vec4 light_space_pos;
} fs_in;

// TBN transforms from tangent to world space.
mat3 calculate_tbn_matrix(vec4 tangent_sign, vec3 normal)
{
    normal = normalize(normal);
    vec3 tangent = normalize(tangent_sign.xyz);
    float bitangent_sign = tangent_sign.w;

    vec3 bitangent = cross(normal, tangent) * bitangent_sign;

    return mat3(tangent, bitangent, normal);
}

void main()
{
    float depth = -fs_in.position.z / u_far_clip_distance;
	gl_FragDepth = depth;

	if (u_use_sampler)
        g_base_color_roughness.rgb = texture(u_base_color, fs_in.tex_coords).rgb;
    else
        g_base_color_roughness.rgb = vec3(0.5);

    if (u_use_normal)
    {
        mat3 tbn = calculate_tbn_matrix(fs_in.tangent, fs_in.normal);
        vec3 normal_tangent = texture(u_normal, fs_in.tex_coords).xyz * 2. -1.;
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

}