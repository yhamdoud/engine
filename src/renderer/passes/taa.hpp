#pragma once

#include <fmt/format.h>

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

class TaaPass
{

    struct Uniforms
    {
        glm::mat4 proj;
        glm::mat4 proj_inv;
        glm::ivec2 size{};
        int filter;
        int flags;
    };

    struct RenderArgs
    {
        glm::mat4 proj;
        glm::mat4 proj_inv;
        glm::ivec2 size;
        uint source_tex;
        uint target_tex;
        uint history_tex;
        uint velocity_tex;
        uint depth_tex;
    };

    struct InitArgs
    {
        glm::ivec2 size;
    };

    static constexpr int group_size = 32;

    Uniforms uniform_data;
    uint uniform_buf;

    uint32_t jitter_idx = 0;

    Shader resolve_shader = *Shader::from_comp_path(
        shaders_path / "taa_resolve.comp",
        fmt::format("#define LOCAL_SIZE {}\n", group_size));

  public:
    enum class Filter : int
    {
        none,
        mitchell,
        blackman_harris,
    };

    enum Flags : int
    {
        none = 0,
        jitter = 1,
        variance_clip = 2,
        velocity_at_closest_depth = 4,
        sharpen = 8,
    };

    struct Params
    {
        int jitter_sample_count;
        Flags flags;
        Filter filter;
    };

    bool enabled = false;
    Params params;

    TaaPass(Params params);

    void parse_params();
    void init(InitArgs &args);
    void render(const RenderArgs &args);
};

} // namespace engine