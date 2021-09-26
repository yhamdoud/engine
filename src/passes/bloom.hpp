#pragma once

#include "../constants.hpp"
#include "../pass.hpp"
#include "../render_context.hpp"
#include "../shader.hpp"

namespace engine
{

struct BloomConfig
{
    uint pass_count;
    float intensity;
    float upsample_radius;
};

class BloomPass
{
    uint downsample_tex = invalid_texture_id;
    uint upsample_tex = invalid_texture_id;

    Shader downsample;
    Shader downsample_hq;
    Shader upsample;

  public:
    BloomConfig cfg;

    BloomPass(BloomConfig cfg);

    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &r_ctx);
};

static_assert(Pass<BloomPass>);

} // namespace engine