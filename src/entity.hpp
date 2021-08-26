#pragma once

#include <cstdint>

#include "model.hpp"
#include "shader.hpp"
#include "transform.hpp"

namespace engine
{

struct Entity
{
    enum Flags : uint32_t
    {
        none = 0,
        casts_shadow = 1 << 0,
    };

    Flags flags = Flags::none;
    size_t scene_graph_index;
    size_t mesh_index;
    Shader &shader;
};

} // namespace engine
