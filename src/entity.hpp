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
    uint base_color_tex_id;
    uint normal_tex_id;
    uint metallic_roughness_tex_id;
    float metallic_factor;
    float roughness_factor;
    glm::vec3 base_color_factor;
    AlphaMode alpha_mode;
    float alpha_cutoff;

    Shader &shader;
};

} // namespace engine
