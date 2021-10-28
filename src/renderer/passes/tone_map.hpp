#pragma once

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

class ToneMapPass
{
    struct Uniforms
    {
        glm::uvec2 size;
        float min_log_luminance;
        float log_luminance_range;
        float log_luminance_range_inverse;
        float dt;
        float exposure_adjust_speed;
        float target_luminance;
        float gamma;
        uint tm_operator;
        float pad0;
        float pad1;
    };

    static constexpr uint histogram_size = 256;
    static constexpr glm::ivec2 group_size{16, 16};

    Shader tonemap_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "lighting.vs",
        .frag = shaders_path / "tonemap.fs",
    });
    Shader histogram_shader = *Shader::from_comp_path(
        shaders_path / "tone_map_histogram.comp", "#define LOCAL_SIZE 16\n");
    Shader reduction_shader = *Shader::from_comp_path(
        shaders_path / "tone_map_reduction.comp", "#define BIN_COUNT 256\n");

    uint histogram_buf;
    uint luminance_buf;
    uint uniform_buf;

    Uniforms uniform_data;

  public:
    enum Operator : int
    {
        none,
        reinhard,
        aces,
        uncharted2,
    };

    struct Params
    {
        Operator tm_operator;
        float gamma;
        float min_log_luminance;
        float max_log_luminance;
        float exposure_adjust_speed;
        float target_luminance;
        bool auto_exposure;
    };

    Params params;

    ToneMapPass(Params params);

    void parse_params();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &ctx_r);
};

} // namespace engine