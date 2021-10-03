#pragma once

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

struct SsrConfig
{
    float thickness;
    int stride;
    bool do_jitter;
    float max_dist;
    int max_steps;
};

class SsrPass
{
    uint frame_buf;
    Shader ssr;
    Shader blend;

  public:
    SsrConfig cfg;
    uint ssr_tex;

    SsrPass(SsrConfig cfg);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx, RenderContext &ctx_r);
};

} // namespace engine