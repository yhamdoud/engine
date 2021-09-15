#pragma once

#include <variant>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.hpp"
#include "entity.hpp"
#include "model.hpp"
#include "shader.hpp"

namespace engine
{

struct MeshInstance
{
    uint buffer_id;
    int primitive_count;
    int positions_offset;
    int normals_offset;
    int tex_coords_offset;
    int tangents_offset;
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
    Shader &shader;
};

struct Light
{
    glm::vec3 position;
    glm::vec3 color;
};

struct DirectionalLight
{
    glm::vec3 position;
    glm::vec3 color;
    // Intensity is illuminance at perpendicular incidence in lux.
    float intensity;
    glm::vec3 direction;
};

struct PostProcessingConfig
{
    bool do_tonemap;
    bool do_gamma_correct;
    float exposure;
    float gamma;
};

struct DebugConfig
{
    bool draw_probes;
    bool use_indirect_illumination;
    bool use_direct_illumination;
    bool use_base_color;
};

struct GBuffer
{
    glm::ivec2 size;
    uint framebuffer;
    uint base_color_roughness;
    uint normal_metallic;
    uint depth;
};

struct IrradianceProbe
{
    glm::vec3 position;
    uint cubemap;
};

class Renderer
{
    enum class Error
    {
        unsupported_texture_format,
    };

    std::array<Light, 3> point_lights = {
        //        Light{glm::vec3{0, 3, 0}, glm::vec3{20.f, 10.f, 10.f}},
        Light{glm::vec3{0, 3, 3}, glm::vec3{0., 0., 0.}},
        Light{glm::vec3{0, 3, 3}, glm::vec3{0., 0., 0.}},
        Light{glm::vec3{3, 3, 3}, glm::vec3{0., 0., 0.}},
    };

    std::vector<MeshInstance> mesh_instances;

    // FIXME:
    unsigned int vao_entities;
    unsigned int vao_skybox;

    Shader shadow_shader;

    Shader skybox_shader;
    unsigned int texture_skybox;

    uint fbo_shadow;
    Shader lighting_shader;

    size_t probe_mesh_idx;

    GBuffer create_g_buffer(glm::ivec2 size);

  public:
    glm::ivec2 shadow_map_size{2048, 2048};
    uint shadow_map;

    // Projection settings
    float far_clip_distance = 50.f;

    DirectionalLight sun{
        .position = glm::vec3(0.f, 15., -5.f),
        .color = glm::vec3{1.f, 0.95f, 0.95f},
        .intensity = 40.f,
        .direction = glm::normalize(glm::vec3{0.2f, -1.f, 0.2f}),
    };

    PostProcessingConfig post_proc_cfg{
        .do_tonemap = true,
        .do_gamma_correct = true,
        .exposure = 1.0,
        .gamma = 2.2f,
    };

    DebugConfig debug_cfg{
        .draw_probes = false,
        .use_indirect_illumination = true,
        .use_direct_illumination = true,
        .use_base_color = true,
    };

    // Deferred
    GBuffer g_buf;

    uint debug_view_base_color;
    uint debug_view_roughness;
    uint debug_view_normal;
    uint debug_view_metallic;

    // GI
    glm::ivec2 probe_env_map_size{64, 64};
    GBuffer g_buf_probe;
    uint fbo_probe;
    uint probe_env_map;
    Shader probe_debug_shader;
    Shader probe_shader;
    std::vector<IrradianceProbe> probes;

    // Post processing
    uint hdr_screen;
    uint hdr_fbo;

    Shader tonemap_shader;

    glm::ivec2 viewport_size;
    Camera camera{glm::vec3{0, 0, 4}, glm::vec3{0}};

    Renderer(glm::ivec2 viewport_size, Shader skybox,
             unsigned int skybox_texture);
    ~Renderer();

    size_t register_mesh(const Mesh &mesh);

    void render_mesh_instance(unsigned int vao, const MeshInstance &mesh);
    void render(std::vector<RenderData> &queue);

    IrradianceProbe generate_probe(glm::vec3 position,
                                   std::vector<RenderData> &queue,
                                   bool use_indirect);
    void generate_probe_grid_gpu(std::vector<RenderData> &queue,
                                 glm::vec3 center, glm::vec3 world_dims,
                                 float distance);
    void generate_probe_grid_cpu(std::vector<RenderData> &queue,
                                 glm::vec3 center, glm::vec3 world_dims,
                                 float distance);

    std::vector<uint> sh_texs;
    glm::mat4 inv_grid_transform;
    glm::vec3 grid_dims;

    void resize_viewport(glm::vec2 size);

    std::variant<uint, Error> register_texture(const Texture &texture);
    void geometry_pass(std::vector<RenderData> &queue, const glm::mat4 &proj,
                       const glm::mat4 &view);
    void lighting_pass(const glm::mat4 &proj, const glm::mat4 &view,
                       const GBuffer &g_buf, const glm::mat4 &light_transform,
                       bool use_indirect);
    void forward_pass(const glm::mat4 &proj, const glm::mat4 &view);
    void shadow_pass(std::vector<RenderData> &queue,
                     const glm::mat4 &light_transform);
};

} // namespace engine