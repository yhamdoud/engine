#include <glad/glad.h>
#include <glm/glm.hpp>

#include "logger.hpp"
#include "profiler.hpp"
#include "renderer/context.hpp"
#include "ssr.hpp"

using namespace std;
using namespace glm;
using namespace engine;

SsrPass::SsrPass(SsrConfig cfg)
    : ssr(*Shader::from_paths({.vert = shaders_path / "lighting.vs",
                               .frag = shaders_path / "ssr.fs"})),
      cfg(cfg)
{
    parse_parameters();
}

void SsrPass::parse_parameters()
{
    ssr.set("u_thickness", cfg.thickness);
    ssr.set("u_stride", static_cast<float>(cfg.stride));
    ssr.set("u_do_jitter", cfg.do_jitter);
    ssr.set("u_max_dist", cfg.max_dist);
    ssr.set("u_max_steps", cfg.max_steps);
}

void SsrPass::initialize(ViewportContext &ctx)
{
    glCreateTextures(GL_TEXTURE_2D, 1, &ctx.reflections_tex);
    glCreateFramebuffers(1, &frame_buf);

    // TODO:
    glTextureParameteri(ctx.reflections_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(ctx.reflections_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureStorage2D(ctx.reflections_tex, 1, GL_RGBA8, ctx.size.x,
                       ctx.size.y);

    array<GLenum, 1> draw_bufs{GL_COLOR_ATTACHMENT0};
    glNamedFramebufferTexture(frame_buf, draw_bufs[0], ctx.reflections_tex, 0);
    glNamedFramebufferDrawBuffers(frame_buf, draw_bufs.size(),
                                  draw_bufs.data());

    if (glCheckNamedFramebufferStatus(frame_buf, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
        logger.error("SSR framebuffer incomplete");
}

void SsrPass::render(ViewportContext &ctx, RenderContext &ctx_r)
{
    ZoneScoped;

    glViewport(0, 0, ctx.size.x, ctx.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buf);
    glClear(GL_COLOR_BUFFER_BIT);

    ssr.set("u_proj_inv", ctx.proj_inv);
    ssr.set("u_proj", ctx.proj);
    ssr.set("u_near", ctx.near);

    glBindTextureUnit(0, ctx.g_buf.depth);
    glBindTextureUnit(1, ctx.g_buf.normal_metallic);
    glBindTextureUnit(2, ctx.g_buf.base_color_roughness);

    glUseProgram(ssr.get_id());
    glDrawArrays(GL_TRIANGLES, 0, 3);
}