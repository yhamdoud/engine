#include <Tracy.hpp>
#include <glad/glad.h>

#include "bloom.hpp"

using namespace engine;
using namespace std;

BloomPass::BloomPass(BloomConfig cfg) : cfg(cfg)
{
    downsample =
        *Shader::from_comp_path(shaders_path / "bloom_downsample.comp");

    downsample_hq =
        *Shader::from_comp_path(shaders_path / "bloom_downsample_hq.comp");
    upsample = *Shader::from_comp_path(shaders_path / "bloom_upsample.comp");
}

void BloomPass::initialize(ViewportContext &ctx)
{
    glCreateTextures(GL_TEXTURE_2D, 1, &downsample_tex);
    glTextureStorage2D(downsample_tex, cfg.pass_count, GL_RGBA16F,
                       ctx.size.x / 2, ctx.size.y / 2);
    glTextureParameteri(downsample_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(downsample_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(downsample_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(downsample_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glCreateTextures(GL_TEXTURE_2D, 1, &upsample_tex);
    glTextureStorage2D(upsample_tex, cfg.pass_count - 1, GL_RGBA16F,
                       ctx.size.x / 2, ctx.size.y / 2);
    glTextureParameteri(upsample_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(upsample_tex, GL_TEXTURE_MIN_FILTER,
                        GL_NEAREST_MIPMAP_LINEAR);
    glTextureParameteri(upsample_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(upsample_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void BloomPass::render(ViewportContext &ctx_v, RenderContext &r_ctx)
{
    ZoneScoped;

    Shader &downsample_shader = downsample_hq;

    const uint upsample_count = cfg.pass_count - 1;

    glUseProgram(downsample_shader.get_id());

    glBindTextureUnit(0u, ctx_v.hdr_tex2);

    for (uint i = 0u; i < cfg.pass_count; i++)
    {
        // Round up.
        const uint group_count_x = ((ctx_v.size.x >> i + 1u) + 15u) / 16u;
        const uint group_count_y = ((ctx_v.size.y >> i + 1u) + 15u) / 16u;

        downsample_shader.set("u_level", i);

        glBindImageTexture(1u, downsample_tex, i, false, 0, GL_WRITE_ONLY,
                           GL_RGBA16F);
        glDispatchCompute(group_count_x, group_count_y, 1);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        if (i == 0)
            glBindTextureUnit(0u, downsample_tex);
    }

    glUseProgram(upsample.get_id());

    upsample.set("u_intensity", 1.f);
    upsample.set("u_radius", cfg.upsample_radius);

    glBindTextureUnit(0u, downsample_tex);
    glBindTextureUnit(1u, downsample_tex);

    for (int i = upsample_count - 1; i >= 0; i--)
    {
        const uint group_count_x = ((ctx_v.size.x >> i + 1u) + 15u) / 16u;
        const uint group_count_y = ((ctx_v.size.y >> i + 1u) + 15u) / 16u;

        upsample.set("u_level", (uint)i + 1u);
        upsample.set("u_target_level", (uint)i);

        glBindImageTexture(2u, upsample_tex, i, false, 0, GL_WRITE_ONLY,
                           GL_RGBA16F);
        glDispatchCompute(group_count_x, group_count_y, 1u);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        if (i == upsample_count - 1)
            glBindTextureUnit(0u, upsample_tex);
    }

    uint group_count_x = ctx_v.size.x / 16u;
    uint group_count_y = ctx_v.size.y / 16u;

    upsample.set("u_level", 0u);
    upsample.set("u_target_level", 0u);
    upsample.set("u_intensity", cfg.intensity);

    glBindTextureUnit(1u, ctx_v.hdr_tex2);
    glBindImageTexture(2u, ctx_v.hdr_tex, 0, false, 0, GL_READ_WRITE,
                       GL_RGBA16F);

    glDispatchCompute(group_count_x, group_count_y, 1u);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}