#pragma once

#include "../constants.hpp"
#include "../render_context.hpp"
#include "../shader.hpp"

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

class SsaoPass
{
    uint frame_buf;
    std::array<glm::vec3, 64> kernel;
    uint ao_tex;
    uint ao_blurred_tex;
    uint noise_tex;
    Shader ssao;
    Shader blur;
    int kernel_size;

  public:
    int sample_count;
    float radius;
    float bias;
    float strength;

    uint debug_view_ssao;

    SsaoPass(SsaoConfig cfg);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx);
};

} // namespace engine