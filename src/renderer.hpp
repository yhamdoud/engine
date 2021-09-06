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

class Renderer
{
    enum class Error
    {
        unsupported_texture_format,
    };

    std::array<Light, 3> point_lights = {
        Light{glm::vec3{0, 3, 0}, glm::vec3{20.f, 10.f, 10.f}},
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

    void create_g_buffer();

  public:
    glm::ivec2 shadow_map_size{2048, 2048};
    uint shadow_map;

    // Projection settings
    float far_clip_distance = 50.f;

    DirectionalLight sun{
        .position = glm::vec3(0.f, 15., -5.f),
        .color = glm::vec3{1.f, 0.95f, 0.95f},
        //        .intensity = 100'000.f,
        .intensity = 20.f,
        .direction = glm::normalize(glm::vec3{0.f, -1.f, 0.2f}),
    };

    PostProcessingConfig post_proc_cfg{
        .do_tonemap = true,
        .do_gamma_correct = true,
        .exposure = 1.0,
        .gamma = 2.2f,
    };

    // Deferred
    uint g_buffer;
    uint g_depth;
    uint g_normal_metallic;
    uint g_base_color_roughness;

    uint debug_view_base_color;
    uint debug_view_roughness;
    uint debug_view_normal;
    uint debug_view_metallic;

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
    void render(std::vector<RenderData> &m);

    void resize_viewport(glm::vec2 size);

    std::variant<uint, Error> register_texture(const Texture &texture);
};

} // namespace engine