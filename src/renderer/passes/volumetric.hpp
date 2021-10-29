#pragma once

#include <cstdint>

#include <fmt/format.h>

#include "constants.hpp"
#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

class VolumetricPass
{
    struct Uniforms
    {
        glm::mat4 proj{};
        glm::mat4 proj_inv{};
        glm::mat4 view_inv{};
        glm::uvec2 size{};
        int count = 0;
        float scatter_intensity = 0.f;
        glm::vec3 sun_dir{};
        bool bilateral_upsample = false;
        glm::vec3 sun_color{};
        float scatter_amount = 0.f;
    };

    static constexpr int group_size = 32;

    Shader raymarch_shader = *Shader::from_comp_path(
        shaders_path / "volumetric.comp",
        fmt::format("#define LOCAL_SIZE {}\n#define CASCADE_COUNT 3\n",
                    group_size));

    Shader blur_horizontal_shader = *Shader::from_comp_path(
        shaders_path / "volumetric_blur.comp",
        fmt::format("#define LOCAL_SIZE {}\n#define HORIZONTAL\n", group_size));

    Shader blur_vertical_shader = *Shader::from_comp_path(
        shaders_path / "volumetric_blur.comp",
        fmt::format("#define LOCAL_SIZE {}\n#define VERTICAL\n", group_size));

    Shader upsample_shader = *Shader::from_comp_path(
        shaders_path / "volumetric_upsample.comp",
        fmt::format("#define LOCAL_SIZE {}\n", group_size));

    Uniforms uniform_data;

    uint uniform_buf;

    uint tex1 = invalid_texture_id;
    uint tex2 = invalid_texture_id;

  public:
    bool enabled = true;

    enum Flags : int
    {
        none,
        bilateral_upsample,
        blur,
    };

    struct Params
    {
        int step_count;
        float scatter_intensity;
        float scatter_amount;
        Flags flags;
    };

    Params params;

    VolumetricPass(Params params);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &ctx_r);
};

} // namespace engine