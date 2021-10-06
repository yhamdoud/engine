#version 460 core

out vec4 frag_color;

uniform vec3 u_light_pos;
uniform mat4 u_model_view;

uniform sampler2D u_shadow_map;

in Varying
{
    // View space vectors.
    vec3 position;
    vec3 normal;
    vec2 tex_coords;
    vec4 light_space_pos;
} fs_in;

// Source: https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
float calculate_shadow(vec4 pos_light_space, vec3 normal, vec3 light_dir)
{
    vec3 pos = pos_light_space.xyz / pos_light_space.w;

    // Handle points beyond the far plane of the light's frustrum.
    if (pos.z > 1.0)
        return 0.;

    // Transform NDC to texture space.
    pos = 0.5 * pos + 0.5;

    float bias_max = 0.03;
    float bias_min = 0.001;

    float bias = max(bias_max * (1 - dot(normal, light_dir)), bias_min);

    // PCF
    vec2 scale = 1. / textureSize(u_shadow_map, 0);

    int range = 3;
    int sample_count = (2 * range + 1) * (2 * range + 1);

    float shadow = 0.;

    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            vec2 offset = vec2(x, y);
            float depth = texture(u_shadow_map, pos.xy + scale * offset).r;
            shadow += int(pos.z - bias > depth);
        }
    }

    return shadow / float(sample_count);
}

void main()
{
    // TODO: Differentiate between point and directional lights.
    vec3 l = normalize(u_light_pos - fs_in.position);
    vec3 v = normalize(-fs_in.position);
    vec3 n = normalize(fs_in.normal);
    vec3 h = normalize(l + v);

    float shadow = calculate_shadow(fs_in.light_space_pos, n, l) > 0. ? 1. : 0;

    float ambient = 0.3;
    float diffuse = smoothstep(0., 0.05, dot(n, l) * (1. - shadow));
    float spec = smoothstep(0.005, 0.01, pow(max(dot(n, h), 0), 32));

    vec3 ambient_color = vec3(0, 1, 0);
    vec3 diffuse_color = vec3(0, 1, 0);
    vec3 light_color = vec3(1);

    vec3 col = ambient * ambient_color + diffuse * diffuse_color + spec * light_color;

    frag_color = vec4(col, 1);
}