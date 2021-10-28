#ifndef UNIFORMS_H
#define UNIFORMS_H

struct VolumetricUniforms
{
    mat4 proj;
    mat4 proj_inv;
    mat4 view_inv;
    uvec2 read_size;
    int step_count;
    float scatter_intensity;
    vec3 sun_dir;
    bool bilateral_upsample;
    vec3 sun_color;
    float scatter_amount;
};

struct ToneMapUniforms
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
};

struct LightingUniforms
{
    mat4 proj;
    mat4 proj_inv;
    mat4 view_inv;
    mat4 inv_grid_transform;
    float leak_offset;
    bool indirect_lighting;
    bool direct_lighting;
    bool base_color;
    bool color_cascades;
    bool filter_shadows;
    bool reflections;
    bool ambient_occlusion;
    vec3 grid_dims;
    vec3 light_intensity;
    vec3 light_direction;
};

#endif