#version 460 core

in vec2 tex_coords;

layout(binding = 0) uniform sampler2D u_ao;
layout(location = 1) out float frag_occlusion;

const int range = 4;

void main()
{
    const vec2 texel_size = 1. / vec2(textureSize(u_ao, 0));
    const vec2 origin = -vec2(float(range) * 0.5 - 0.5);

    float result = 0.0;

    for (int y = 0; y < range; ++y)
    {
        for (int x = 0; x < range; ++x)
        {
            const vec2 offset = (origin + vec2(x, y)) * texel_size;
            result += texture(u_ao, tex_coords + offset).r;
        }
    }

    frag_occlusion = result / float(4 * 4);
}
