#ifndef COMMON_H
#define COMMON_H

// https://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer/
float linearize_depth(float depth, mat4 proj)
{
    return -proj[3][2] / (2. * depth - 1. + proj[2][2]);
}

void swap(inout float a, inout float b)
{
    float t = a;
    a = b;
    b = t;
}

float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

float luma(vec4 c) { return luma(vec3(c)); }

float saturate(float x) { return clamp(x, 0., 1.); }

vec3 saturate(vec3 x) { return clamp(x, 0., 1.); }

float distance_squared(vec2 a, vec2 b)
{
    a -= b;
    return dot(a, a);
}

vec2 calc_tex_coords(ivec2 index, ivec2 size)
{
    return (vec2(index) + 0.5) / vec2(size);
}

vec2 calc_tex_coords(uvec2 index, uvec2 size)
{
    return (vec2(index) + 0.5) / vec2(size);
}

#endif