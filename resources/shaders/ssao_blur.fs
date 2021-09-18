#version 460 core

in vec2 tex_coords;

layout (binding = 0) uniform sampler2D u_ao;
layout (location = 1) out float frag_occlusion;

const int x_range = 2;
const int y_range = 2;

void main()
{
    float result = 0.0;
    for (int x = -x_range; x < x_range; x++) 
    {
        for (int y = -y_range; y < y_range; y++) 
        {
            vec2 offset = vec2(x, y) / vec2(textureSize(u_ao, 0));
            result += texture(u_ao, tex_coords + offset).r;
        }
    }

    frag_occlusion = result / float(2 * x_range * 2 * y_range);
}
