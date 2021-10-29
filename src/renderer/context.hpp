#pragma once

#include <glm/glm.hpp>

#include "constants.hpp"
#include "entity.hpp"
#include "model.hpp"
#include "renderer/buffer.hpp"
#include "renderer/light.hpp"

namespace engine
{

struct GBuffer
{
    glm::ivec2 size{0};
    uint framebuffer = default_frame_buffer_id;
    uint base_color_roughness = invalid_texture_id;
    uint normal_metallic = invalid_texture_id;
    uint velocity = invalid_texture_id;
    uint depth = invalid_texture_id;
};

struct MeshInstance
{
    uint32_t vertex_offset = 0u;
    uint64_t index_offset_bytes = 0u;
    int primitive_count = 0;
};

struct ViewportContext
{
    glm::ivec2 size{0};
    uint hdr_tex = invalid_texture_id;
    uint hdr2_tex = invalid_texture_id;
    uint id_tex = invalid_texture_id;
    uint hdr_prev_tex = invalid_texture_id;
    uint hdr_frame_buf = default_frame_buffer_id;
    uint ldr_frame_buf = default_frame_buffer_id;
    uint geometry_fbuf = default_frame_buffer_id;
    glm::mat4 proj{0.f};
    glm::mat4 proj_inv{0.f};
    glm::mat4 view{0.f};
    glm::mat4 view_inv{0.f};
    glm::mat4 view_proj{0.f};
    glm::mat4 view_proj_prev{0.f};
    float near = 0;
    float far = 0;
    float fov = 0;
    GBuffer g_buf{};
    uint ao_tex = invalid_texture_id;
    uint shadow_map = invalid_texture_id;
    uint reflections_tex = invalid_texture_id;
    std::span<float> cascade_distances{};
    std::span<glm::mat4> light_transforms{};
};

struct RenderContext
{
    DirectionalLight sun{};
    std::vector<MeshInstance> mesh_instances{};
    std::vector<Entity> queue{};
    std::vector<Light> lights{};
    uint entity_vao = invalid_texture_id;
    uint skybox_vao = invalid_texture_id;
    uint skybox_tex = invalid_texture_id;
    glm::mat4 inv_grid_transform{0.f};
    glm::vec3 grid_dims{0.f};
    std::span<uint, 7> sh_texs;
    std::vector<glm::vec3> probes{};
    size_t sphere_mesh_idx = -1;
    float dt = 0.f;
    Buffer vertex_buf;
    Buffer index_buf;
};

struct BakingJob
{
    glm::ivec3 coords;
};

} // namespace engine