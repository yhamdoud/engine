#include <glm/glm.hpp>

#include "logger.hpp"
#include "renderer/passes/volumetric.hpp"

using namespace std;
using namespace engine;
using namespace glm;

VolumetricPass::VolumetricPass(Params params) : params(params)
{
    glCreateBuffers(1, &uniform_buf);
    glNamedBufferData(uniform_buf, sizeof(Uniforms), nullptr, GL_DYNAMIC_DRAW);

    parse_parameters();
}

void VolumetricPass::parse_parameters()
{
    bool x = params.flags & Flags::bilateral_upsample;
    uniform_data = {
        .count = params.step_count,
        .scatter_intensity = params.scatter_intensity,
        .bilateral_upsample = x,
        .scatter_amount = params.scatter_amount,
    };
}

void VolumetricPass::initialize(ViewportContext &ctx)
{
    glDeleteTextures(2, &tex1);
    glCreateTextures(GL_TEXTURE_2D, 2, &tex1);

    glTextureParameteri(tex1, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex1, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTextureParameteri(tex2, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex2, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTextureStorage2D(tex1, 1, GL_RGBA16F, ctx.size.x / 2, ctx.size.y / 2);
    glTextureStorage2D(tex2, 1, GL_RGBA16F, ctx.size.x / 2, ctx.size.y / 2);
}

void VolumetricPass::render(ViewportContext &ctx_v, RenderContext &ctx_r,
                            uint target_tex)
{
    uniform_data.proj = ctx_v.proj;
    uniform_data.proj_inv = ctx_v.proj_inv;
    uniform_data.view_inv = inverse(ctx_v.view);
    uniform_data.size = ctx_v.size;
    uniform_data.sun_dir =
        normalize(vec3(ctx_v.view * vec4(ctx_r.sun.direction, 0.f)));
    uniform_data.sun_color = ctx_r.sun.color * ctx_r.sun.intensity;
    glNamedBufferSubData(uniform_buf, 0, sizeof(Uniforms), &uniform_data);

    // FIXME:
    raymarch_shader.set("u_light_transforms[0]", span(ctx_v.light_transforms));
    raymarch_shader.set("u_cascade_distances[0]",
                        span(ctx_v.cascade_distances));

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniform_buf);
    glBindTextureUnit(1, ctx_v.g_buf.depth);
    glBindTextureUnit(2, ctx_v.shadow_map);
    glBindTextureUnit(3, tex1);
    glBindTextureUnit(4, tex2);
    glBindImageTexture(5, tex1, 0, false, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(6, tex2, 0, false, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(7, target_tex, 0, false, 0, GL_READ_WRITE, GL_RGBA16F);

    uvec2 half_group_count =
        (ctx_v.size / 2 + ivec2(group_size) - 1) / ivec2(group_size);

    uvec2 group_count =
        (ctx_v.size + ivec2(group_size) - 1) / ivec2(group_size);

    glUseProgram(raymarch_shader.get_id());
    glDispatchCompute(half_group_count.x, half_group_count.y, 1u);

    if (params.flags & Flags::blur)
    {
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

        glUseProgram(blur_horizontal_shader.get_id());
        glDispatchCompute(half_group_count.x, half_group_count.y, 1u);

        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

        glUseProgram(blur_vertical_shader.get_id());
        glDispatchCompute(half_group_count.x, half_group_count.y, 1u);
    }

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glUseProgram(upsample_shader.get_id());
    glDispatchCompute(group_count.x, group_count.y, 1u);

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}