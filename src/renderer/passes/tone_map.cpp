#include <Tracy.hpp>
#include <glad/glad.h>

#include "tone_map.hpp"

using namespace engine;

ToneMapPass::ToneMapPass(ToneMapConfig cfg)
    : do_tone_map(cfg.do_tonemap), do_gamma_correct(cfg.do_gamma_correct),
      exposure(cfg.exposure), gamma(cfg.gamma)
{
    tonemap_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "lighting.vs",
        .frag = shaders_path / "tonemap.fs",
    });

    parse_parameters();
}

void ToneMapPass::parse_parameters()
{
    tonemap_shader.set("u_hdr_screen", 0);
    tonemap_shader.set("u_do_tonemap", do_tone_map);
    tonemap_shader.set("u_do_gamma_correct", do_gamma_correct);
    tonemap_shader.set("u_exposure", exposure);
    tonemap_shader.set("u_gamma", gamma);
}

void ToneMapPass::initialize(ViewportContext &ctx) {}

void ToneMapPass::render(ViewportContext &ctx_v, RenderContext &ctx_r)
{
    ZoneScoped;

    glBindFramebuffer(GL_FRAMEBUFFER, ctx_v.ldr_frame_buf);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(tonemap_shader.get_id());
    glBindTextureUnit(0, ctx_v.hdr_tex);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}