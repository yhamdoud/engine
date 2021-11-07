#include "renderer/passes/sharpen.hpp"

using namespace engine;
using namespace glm;

SharpenPass::SharpenPass() {}

void SharpenPass::render(const RenderArgs &args)
{
    glBindImageTexture(0, args.target_tex, 0, false, 0, GL_WRITE_ONLY,
                       GL_RGBA16F);
    glBindTextureUnit(0, args.source_tex);

    uvec2 group_count = (args.size + ivec2(group_size) - 1) / ivec2(group_size);

    glUseProgram(sharpen_shader.get_id());
    glDispatchCompute(group_count.x, group_count.y, 1u);
}