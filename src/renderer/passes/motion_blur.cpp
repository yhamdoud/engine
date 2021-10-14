#include "renderer/passes/motion_blur.hpp"
#include "profiler.hpp"

using namespace engine;
using namespace std;

MotionBlurPass::MotionBlurPass(MotionBlurConfig cfg) : cfg(cfg)
{
    parse_parameters();
}

void MotionBlurPass::parse_parameters()
{
    shader.set("u_sample_count", cfg.sample_count);
    shader.set("u_intensity", cfg.intensity);
}

void MotionBlurPass::initialize(ViewportContext &ctx) {}

void MotionBlurPass::render(ViewportContext &ctx_v, RenderContext &ctx_r)
{
    ZoneScoped;

    glBindTextureUnit(0u, ctx_v.g_buf.velocity);
    glBindTextureUnit(1u, ctx_v.hdr_tex);
    glBindImageTexture(2u, ctx_v.hdr2_tex, 0, false, 0, GL_WRITE_ONLY,
                       GL_RGBA16F);

    glUseProgram(shader.get_id());

    glDispatchCompute((ctx_v.size.x + 31) / 32, (ctx_v.size.y + 31) / 32, 1);
}