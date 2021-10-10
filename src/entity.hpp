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
    size_t mesh_index;
    glm::mat4 model;
    Material material;
};

struct SceneGraphNode
{
    static constexpr size_t root_index = (size_t)-1;

    glm::mat4 local_transform_matrix;
    size_t parent_index = root_index;

    void set_position(const glm::vec3 &pos)
    {
        local_transform_matrix[3] =
            glm::vec4{pos, local_transform_matrix[3][3]};
    }

    SceneGraphNode(glm::mat4 m, size_t i)
        : local_transform_matrix{m}, parent_index{i}
    {
    }
};

} // namespace engine
