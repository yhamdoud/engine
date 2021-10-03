#version 460 core

in vec3 normal;
in vec3 position;

uniform mat4 u_inv_grid_transform;
uniform vec3 u_grid_dims;

uniform sampler3D u_sh_0;
uniform sampler3D u_sh_1;
uniform sampler3D u_sh_2;
uniform sampler3D u_sh_3;
uniform sampler3D u_sh_4;
uniform sampler3D u_sh_5;
uniform sampler3D u_sh_6;


out vec4 frag_color;

void main()
{
    vec3 n = normalize(normal);

    vec3 grid_coords = (u_inv_grid_transform * vec4(position, 1.f)).xyz;
    vec3 tex_coords = (grid_coords + vec3(0.5f)) / u_grid_dims;
    vec3 radiance = vec3(0.f);

    vec4 c0 = texture(u_sh_0, tex_coords);
    vec4 c1 = texture(u_sh_1, tex_coords);
    vec4 c2 = texture(u_sh_2, tex_coords);
    vec4 c3 = texture(u_sh_3, tex_coords);
    vec4 c4 = texture(u_sh_4, tex_coords);
    vec4 c5 = texture(u_sh_5, tex_coords);
    vec4 c6 = texture(u_sh_6, tex_coords);
    vec3 c7 = vec3(c0.w, c1.w, c2.w);
    vec3 c8 = vec3(c3.w, c4.w, c5.w);

    radiance = c0.rgb * 0.282095f +
               c1.rgb * 0.488603f * n.y +
               c2.rgb * 0.488603f * n.z +
               c3.rgb * 0.488603f * n.x +
               c4.rgb * 1.092548f * n.x * n.y +
               c5.rgb * 1.092548f * n.y * n.z +
               c6.rgb * 0.315392f * (3.f * n.z * n.z - 1.f) +
               c7.rgb * 1.092548f * n.x * n.z +
               c8.rgb * 0.546274f * (n.x * n.x - n.y * n.y);

    frag_color = vec4(radiance, 1.f);
}
