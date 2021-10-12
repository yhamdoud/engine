#pragma once

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

struct ToneMapParams
{
    bool do_tone_map;
    bool do_gamma_correct;
    float exposure;
    float gamma;

    float min_log_luminance;
    float max_log_luminance;
    float exposure_adjust_speed;
    float target_luminance;
};

struct ExposureUniforms
{
    glm::uvec2 size;
    float min_log_luminance;
    float log_luminance_range;
    float log_luminance_range_inverse;
    float dt;
    float exposure_adjust_speed;
    float target_luminance;
};

class ToneMapPass
{
    Shader tonemap_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "lighting.vs",
        .frag = shaders_path / "tonemap.fs",
    });
    Shader histogram_shader =
        *Shader::from_comp_path(shaders_path / "tone_map_histogram.comp");
    Shader reduction_shader =
        *Shader::from_comp_path(shaders_path / "tone_map_reduction.comp");

    uint histogram_buf;
    uint luminance_buf;
    uint uniform_buf;
    glm::ivec2 group_size{16, 16};
    uint histogram_size = 256;

    ExposureUniforms uniform_data;

  public:
    ToneMapParams params;

    ToneMapPass(ToneMapParams params);

    void parse_params();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &ctx_r);
};

} // namespace engine