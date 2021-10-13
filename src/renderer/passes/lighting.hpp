#pragma once

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

struct LightingConfig
{
    bool indirect_light;
    bool direct_light;
    bool use_base_color;
    bool color_shadow_cascades;
    bool filter_shadows;
    float leak_offset;
};

class LightingPass
{
    Shader lighting_shader;

  public:
    bool indirect_light;
    bool direct_light;
    bool use_base_color;
    bool color_shadow_cascades;
    bool filter_shadows;
    float leak_offset;

    // FIXME: workaround
    bool ssao = true, ssr = true;

    LightingPass(LightingConfig cfg);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &ctx_r);
};

static_assert(Pass<LightingPass>);

} // namespace engine