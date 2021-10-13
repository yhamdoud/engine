#version 460

in vec2 tex_coords;

layout (std140, binding = 0) uniform Uniforms
{
    uvec2 size;
    float min_log_luminance;
    float log_luminance_range;
    float log_luminance_range_inverse;
    float dt;
    float tau;
    float target_luminance;
	float gamma;
	uint operator;
} u;

layout(binding = 2) uniform sampler2D u_hdr_screen;

layout(std430, binding = 3) buffer Ssbo2 { float luminance_out; };

out vec4 frag_color;

vec3 reinhard(vec3 x)
{
    return x / (1. + x);
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 aces(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0., 1.);
}

// http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 uncharted2_tone_map(vec3 x) {
    float a = 0.15;
    float b = 0.50;
    float c = 0.10;
    float d = 0.20;
    float e = 0.02;
    float f = 0.30;
    float w = 11.2;

    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

vec3 uncharted2(vec3 x) {
    float exposure_bias = 2.0;
    vec3 cur = uncharted2_tone_map(exposure_bias * x);
    const vec3 w = uncharted2_tone_map(vec3(11.2));
    vec3 white_scale = 1.0 / w;

    return cur * white_scale;
}

vec3 gamma_correct(vec3 linear, float gamma)
{
    return pow(linear, vec3(1. / gamma));
}

void main()
{
    vec3 hdr = (u.target_luminance / luminance_out) * texture(u_hdr_screen, tex_coords).rgb;

    vec3 ldr;

    switch (u.operator)
    {
    case 0:
        ldr = hdr; break;
    case 1:
        ldr = reinhard(hdr); break;
    case 2:
        ldr = aces(hdr); break;
    case 3:
        ldr = uncharted2(hdr); break;
    }

    frag_color = vec4(gamma_correct(ldr, u.gamma), 1.);
}