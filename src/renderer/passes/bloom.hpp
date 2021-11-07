#pragma once

#include "constants.hpp"
#include "renderer/pass.hpp"
#include "shader.hpp"

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
    bool enabled = true;

    BloomConfig cfg;

    BloomPass(BloomConfig cfg);

    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &r_ctx, uint source,
                uint target);
};

} // namespace engine