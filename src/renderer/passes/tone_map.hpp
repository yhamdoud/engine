#pragma once

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

struct ToneMapConfig
{
    bool do_tonemap;
    bool do_gamma_correct;
    float exposure;
    float gamma;
};

class ToneMapPass
{
    Shader tonemap_shader;

  public:
    bool do_tone_map;
    bool do_gamma_correct;
    float exposure;
    float gamma;

    ToneMapPass(ToneMapConfig cfg);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &ctx_r);
};

} // namespace engine