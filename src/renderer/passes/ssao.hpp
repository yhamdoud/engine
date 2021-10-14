#pragma once

#include "constants.hpp"
#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

struct SsaoConfig
{
    int kernel_size;
    int sample_count;
    float radius;
    float bias;
    float strength;
};

struct SsaoData
{
    int kernel_size;
    int sample_count;
    float radius;
    float bias;
    glm::mat4 proj{0.f};
    glm::vec2 noise_scale{0.f};
    float strength;
    float pad0 = 0.f;
};

class SsaoPass
{
    std::array<glm::vec3, 64> kernel;

    uint frame_buf;
    uint noise_tex = invalid_texture_id;

    uint ao_tex = invalid_texture_id;
    uint ao_blur_tex = invalid_texture_id;

    Shader ssao = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "lighting.vs",
        .frag = shaders_path / "ssao.fs",
    });
    Shader blur = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "lighting.vs",
        .frag = shaders_path / "ssao_blur.fs",
    });

  public:
    uint ubo;
    SsaoData data;

    SsaoPass(SsaoConfig cfg);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx);
};

} // namespace engine