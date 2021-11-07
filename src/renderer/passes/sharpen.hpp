#pragma once

#include <fmt/format.h>

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

class SharpenPass
{
    struct RenderArgs
    {
        glm::ivec2 size;
        uint source_tex;
        uint target_tex;
    };

    static constexpr int group_size = 32;

    Shader sharpen_shader = *Shader::from_comp_path(
        shaders_path / "sharpen.comp",
        fmt::format("#define LOCAL_SIZE {}\n", group_size));

  public:
    bool enabled = false;

    SharpenPass();

    void render(const RenderArgs &args);
};

} // namespace engine