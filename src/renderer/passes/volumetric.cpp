#include <glm/glm.hpp>

#include "logger.hpp"
#include "renderer/passes/volumetric.hpp"

using namespace std;
using namespace engine;
using namespace glm;

VolumetricPass::VolumetricPass(Params params) : params(params)
{
    parse_parameters();
}

void VolumetricPass::parse_parameters()
{
    uniform_data = {
        .count = params.step_count,
        .scatter_intensity = params.scatter_intensity,
    };

    glCreateBuffers(1, &uniform_buf);
    glNamedBufferData(uniform_buf, sizeof(Uniforms), nullptr, GL_DYNAMIC_DRAW);
}

void VolumetricPass::initialize(ViewportContext &ctx) {}

void VolumetricPass::render(ViewportContext &ctx_v, RenderContext &ctx_r)
{
    uniform_data.proj = ctx_v.proj;
    uniform_data.proj_inv = ctx_v.proj_inv;
    uniform_data.view_inv = inverse(ctx_v.view);
    uniform_data.size = ctx_v.size;
    uniform_data.sun_dir =
        normalize(vec3(ctx_v.view * vec4(ctx_r.sun.direction, 0.f)));
    uniform_data.sun_color = ctx_r.sun.color;
    glNamedBufferSubData(uniform_buf, 0, sizeof(Uniforms), &uniform_data);

    // FIXME:
    shader.set("u_light_transforms[0]", span(ctx_v.light_transforms));
    shader.set("u_cascade_distances[0]", span(ctx_v.cascade_distances));

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniform_buf);
    glBindTextureUnit(1, ctx_v.g_buf.depth);
    glBindTextureUnit(2, ctx_v.shadow_map);
    glBindImageTexture(3, ctx_v.hdr_tex, 0, false, 0, GL_READ_WRITE,
                       GL_RGBA16F);

    uvec2 group_count =
        (ctx_v.size + ivec2(group_size) - 1) / ivec2(group_size);
    glUseProgram(shader.get_id());
    glDispatchCompute(group_count.x, group_count.y, 1u);
}