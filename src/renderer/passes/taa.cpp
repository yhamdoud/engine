#include "renderer/passes/taa.hpp"

using namespace std;
using namespace engine;
using namespace glm;

TaaPass::TaaPass(Params params) : params(params)
{
    glCreateBuffers(1, &uniform_buf);
    glNamedBufferData(uniform_buf, sizeof(Uniforms), nullptr, GL_DYNAMIC_DRAW);

    parse_params();
}

void TaaPass::parse_params()
{
    uniform_data.filter = static_cast<int>(params.filter);
    uniform_data.flags = static_cast<int>(params.flags);
}

void TaaPass::init(InitArgs &args) {}

void TaaPass::render(const RenderArgs &args)
{
    uniform_data.proj = args.proj;
    uniform_data.proj_inv = args.proj_inv;
    uniform_data.size = args.size;
    glNamedBufferSubData(uniform_buf, 0, sizeof(Uniforms), &uniform_data);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniform_buf);

    glBindImageTexture(0, args.target_tex, 0, false, 0, GL_WRITE_ONLY,
                       GL_RGBA16F);

    glBindTextureUnit(0, args.source_tex);
    glBindTextureUnit(1, args.depth_tex);
    glBindTextureUnit(2, args.velocity_tex);
    glBindTextureUnit(3, args.history_tex);

    uvec2 group_count = (args.size + ivec2(group_size) - 1) / ivec2(group_size);

    glUseProgram(resolve_shader.get_id());
    glDispatchCompute(group_count.x, group_count.y, 1u);
}