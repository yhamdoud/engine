#version 460 core

in vec3 tex_coords;
in vec3 position;

uniform samplerCube u_cubemap;
uniform float u_far_clip_distance;

out vec4 frag_color;

void main()
{
    gl_FragDepth = -position.z / u_far_clip_distance;

    frag_color = vec4(texture(u_cubemap, tex_coords).rgb, 1.f);
}
