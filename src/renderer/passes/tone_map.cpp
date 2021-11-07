#include <array>

#include <Tracy.hpp>
#include <glad/glad.h>

#include "tone_map.hpp"

using namespace engine;
using namespace std;
using namespace glm;

ToneMapPass::ToneMapPass(Params params) : params(params) { parse_params(); }

void ToneMapPass::parse_params()
{
    float log_luminance_range =
        abs(params.min_log_luminance - params.max_log_luminance);

    uniform_data = {
        .min_log_luminance = params.min_log_luminance,
        .log_luminance_range = log_luminance_range,
        .log_luminance_range_inverse = 1 / log_luminance_range,
        .exposure_adjust_speed = params.exposure_adjust_speed,
        .target_luminance = params.target_luminance,
        .gamma = params.gamma,
        .tm_operator = static_cast<uint>(params.tm_operator),
    };
}

void ToneMapPass::initialize(ViewportContext &ctx)
{
    vector<uint> hist(histogram_size, 0u);
    glCreateBuffers(1, &histogram_buf);
    glNamedBufferStorage(histogram_buf, histogram_size * sizeof(uint),
                         hist.data(), GL_NONE);

    float zero = 0;
    glCreateBuffers(1, &luminance_buf);
    glNamedBufferStorage(luminance_buf, sizeof(float), &zero, GL_NONE);

    glCreateBuffers(1, &uniform_buf);
    glNamedBufferData(uniform_buf, sizeof(Uniforms), nullptr, GL_DYNAMIC_DRAW);
}

void ToneMapPass::render(ViewportContext &ctx_v, RenderContext &ctx_r,
                         uint source_tex)
{
    ZoneScoped;

    uniform_data.size = ctx_v.size;
    uniform_data.dt = ctx_r.dt;
    glNamedBufferSubData(uniform_buf, 0, sizeof(Uniforms), &uniform_data);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniform_buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, histogram_buf);
    glBindTextureUnit(2, source_tex);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, luminance_buf);

    if (params.auto_exposure)
    {
        uvec2 group_count = (ctx_v.size + group_size - 1) / group_size;
        glUseProgram(histogram_shader.get_id());
        glDispatchCompute(group_count.x, group_count.y, 1u);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glUseProgram(reduction_shader.get_id());
        glDispatchCompute(1u, 1u, 1u);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, ctx_v.ldr_frame_buf);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(tonemap_shader.get_id());
    glDrawArrays(GL_TRIANGLES, 0, 3);
}