#pragma once

#include "fmt/format.h"

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

class VolumetricPass
{
    struct Uniforms
    {
        glm::mat4 proj;
        glm::mat4 proj_inv;
        glm::mat4 view_inv;
        glm::uvec2 size;
        int count;
        float scatter_intensity;
        glm::vec3 sun_dir;
        float pad0;
        glm::vec3 sun_color;
        float pad1;
    };

    static constexpr int group_size = 32;

    Shader shader = *Shader::from_comp_path(
        shaders_path / "volumetric.comp",
        fmt::format("#define LOCAL_SIZE {}\n#define CASCADE_COUNT 3\n",
                    group_size));

    Uniforms uniform_data;

    uint uniform_buf;

  public:
    struct Params
    {
        int step_count;
        float scatter_intensity;
    };

    Params params;

    VolumetricPass(Params params);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &ctx_r);
};

} // namespace engine