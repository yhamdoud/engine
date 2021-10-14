#include <random>

#include <Tracy.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "logger.hpp"
#include "ssao.hpp"

using namespace std;
using namespace glm;
using namespace engine;

SsaoPass::SsaoPass(SsaoConfig cfg)
{
    uniform_real_distribution<float> dist1(0.f, 1.f);
    uniform_real_distribution<float> dist2(-1.f, 1.f);
    default_random_engine gen;

    // Generate samples inside and on the unit hemisphere.
    for (int i = 0; i < kernel.size(); i++)
    {
        // We want samples close to the origin, i.e., we care about
        // occlusions close to the fragment.
        float weight =
            static_cast<float>(i) / static_cast<float>(kernel.size());
        // Accelerating interpolation function.
        weight = lerp(0.1f, 1.f, weight * weight);

        kernel[i] =
            weight * normalize(vec3(dist2(gen), dist2(gen), dist1(gen)));
    }

    // Rotation vectors oriented around the tangent-space surface normal.
    constexpr int noise_count = 16;
    array<vec3, noise_count> noise;

    // These vectors are used to rotate the sampling kernel in an effort
    // to minimize banding artifacts.
    for (int i = 0; i < noise_count; i++)
        noise[i] = normalize(vec3(dist2(gen), dist2(gen), 0.f));

    glCreateTextures(GL_TEXTURE_2D, 1, &noise_tex);
    glTextureStorage2D(noise_tex, 1, GL_RGBA16F, 4, 4);
    glTextureSubImage2D(noise_tex, 0, 0, 0, 4, 4, GL_RGB, GL_FLOAT,
                        noise.data());
    glTextureParameteri(noise_tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(noise_tex, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glCreateBuffers(1, &ubo);
    glNamedBufferData(ubo, sizeof(SsaoData), nullptr, GL_DYNAMIC_DRAW);

    ssao.set("u_kernel[0]", span(kernel));

    data = {
        .kernel_size = cfg.kernel_size,
        .sample_count = cfg.sample_count,
        .radius = cfg.radius,
        .bias = cfg.bias,
        .strength = cfg.strength,
    };

    glCreateFramebuffers(1, &frame_buf);
}

void SsaoPass::initialize(ViewportContext &ctx)
{
    glDeleteTextures(2, &ao_tex);
    glCreateTextures(GL_TEXTURE_2D, 2, &ao_tex);

    glTextureParameteri(ao_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(ao_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTextureStorage2D(ao_tex, 1, GL_R8, ctx.size.x, ctx.size.y);
    glTextureStorage2D(ao_blur_tex, 1, GL_R8, ctx.size.x, ctx.size.y);

    glNamedFramebufferTexture(frame_buf, GL_COLOR_ATTACHMENT0, ao_tex, 0);
    glNamedFramebufferTexture(frame_buf, GL_COLOR_ATTACHMENT1, ao_blur_tex, 0);

    array<GLenum, 2> draw_bufs{
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
    };
    glNamedFramebufferDrawBuffers(frame_buf, draw_bufs.size(),
                                  draw_bufs.data());

    if (glCheckNamedFramebufferStatus(frame_buf, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
        logger.error("SSAO framebuffer incomplete");

    // Clear these so they don't affect probe baking.
    float one = 1.f;
    glClearNamedFramebufferfv(frame_buf, GL_COLOR, 0, &one);
    glClearNamedFramebufferfv(frame_buf, GL_COLOR, 1, &one);

    ctx.ao_tex = ao_blur_tex;
}

void SsaoPass::render(ViewportContext &ctx)
{
    ZoneScoped;

    glViewport(0, 0, ctx.size.x, ctx.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buf);
    // FIXME: Causes artifacts, why?
    // glClear(GL_COLOR_BUFFER_BIT);

    data.proj = ctx.proj;
    data.noise_scale = static_cast<vec2>(ctx.size) / 4.f;

    glNamedBufferSubData(ubo, 0, sizeof(SsaoData), &data);

    ssao.set("u_proj_inv", ctx.proj_inv);

    glBindTextureUnit(0, ctx.g_buf.depth);
    glBindTextureUnit(1, ctx.g_buf.normal_metallic);
    glBindTextureUnit(2, noise_tex);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, ubo);

    glUseProgram(ssao.get_id());
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindTextureUnit(0, ao_tex);

    glUseProgram(blur.get_id());
    glDrawArrays(GL_TRIANGLES, 0, 3);
}