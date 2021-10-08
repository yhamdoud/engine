#pragma once

#include <glm/glm.hpp>

#include "constants.hpp"
#include "entity.hpp"
#include "model.hpp"

namespace engine
{

struct GBuffer
{
    glm::ivec2 size;
    uint framebuffer;
    uint base_color_roughness;
    uint normal_metallic;
    uint velocity;
    uint depth;
};

struct RenderData
{
    Entity::Flags flags;
    size_t mesh_index;
    uint base_color_tex_id;
    uint normal_tex_id;
    uint metallic_roughness_id;
    float metallic_factor;
    float roughness_factor;
    glm::vec3 base_color_factor;
    glm::mat4 model;
    AlphaMode alpha_mode;
    float alpha_cutoff;
};

struct MeshInstance
{
    uint buffer_id;
    int primitive_count;
    int positions_offset;
    int normals_offset;
    int tex_coords_offset;
    int tangents_offset;
};

struct DirectionalLight
{
    glm::vec3 color;
    // Intensity is illuminance at perpendicular incidence in lux.
    float intensity;
    glm::vec3 direction;
};

struct Light
{
    glm::vec3 position;
    glm::vec3 color;
};

struct ViewportContext
{
    glm::ivec2 size;
    uint hdr_tex = invalid_texture_id;
    uint hdr2_tex = invalid_texture_id;
    uint hdr_prev_tex = invalid_texture_id;
    uint hdr_frame_buf = default_frame_buffer_id;
    uint ldr_frame_buf = default_frame_buffer_id;
    glm::mat4 proj;
    glm::mat4 proj_inv;
    glm::mat4 view;
    glm::mat4 view_proj_prev;
    float near = 0;
    float far = 0;
    float fov = 0;
    GBuffer g_buf;
    uint ao_tex = invalid_texture_id;
    uint shadow_map = invalid_texture_id;
    uint reflections_tex = invalid_texture_id;
    std::span<float> cascade_distances;
    std::span<glm::mat4> light_transforms;
};

struct RenderContext
{
    DirectionalLight sun;
    std::vector<MeshInstance> mesh_instances;
    std::vector<RenderData> queue;
    std::vector<Light> lights;
    uint entity_vao = invalid_texture_id;
    uint skybox_vao = invalid_texture_id;
    uint skybox_tex = invalid_texture_id;
    glm::mat4 inv_grid_transform;
    glm::vec3 grid_dims;
    std::span<uint, 7> sh_texs;
    std::vector<glm::vec3> probes;
    size_t probe_mesh_idx;
};

struct BakingJob
{
    glm::ivec3 coords;
};

} // namespace engine