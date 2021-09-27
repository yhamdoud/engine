#pragma once

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

struct ForwardConfig
{
    bool draw_probes;
};

class ForwardPass
{
    Shader skybox_shader;
    Shader probe_shader;

  public:
    bool draw_probes;

    ForwardPass(ForwardConfig cfg);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &ctx_r);
};

static_assert(Pass<ForwardPass>);

} // namespace engine