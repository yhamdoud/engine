#pragma once

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

struct MotionBlurConfig
{
    int sample_count;
    float intensity;
};

class MotionBlurPass
{
    Shader shader = *Shader::from_comp_path(shaders_path / "motion_blur.comp");

  public:
    bool enabled = true;

    MotionBlurConfig cfg;

    MotionBlurPass(MotionBlurConfig cfg);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &ctx_r, uint source,
                uint target);
};

} // namespace engine