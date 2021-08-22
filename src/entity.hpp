#pragma once

#include <cstdint>

#include "model.hpp"
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
    Transform transform;
    std::shared_ptr<Mesh> mesh;
};

} // namespace engine
