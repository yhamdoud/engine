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

class Renderer
{
    enum class Error
    {
        unsupported_texture_format,
    };

    std::array<Light, 3> lights = {
        Light{glm::vec3{3, 3, 0}, glm::vec3{20.f, 20.f, 20.f}},
        Light{glm::vec3{0, 3, 3}, glm::vec3{0., 0., 0.}},
        Light{glm::vec3{3, 3, 3}, glm::vec3{0., 0., 0.}},
    };

    std::vector<MeshInstance> mesh_instances;

    // FIXME:
    unsigned int vao_entities;
    unsigned int vao_skybox;

    Shader shadow_shader;
    glm::ivec2 shadow_map_size{1024, 1024};
    unsigned int texture_shadow;

    Shader skybox_shader;
    unsigned int texture_skybox;

    uint fbo_shadow;
    Shader lighting_shader;

    Shader debug_roughness;
    Shader debug_depth;
    Shader debug_normal;

  public:
    // Projection settings
    float far_clip_distance = 20.f;

    // Deferred
    glm::ivec2 g_buffer_size{1280, 720};
    uint g_buffer;
    uint g_depth;
    uint g_normal;
    uint g_albedo_specular;

    uint debug_view_base_color;
    uint debug_view_roughness;
    uint debug_view_normal;
    uint debug_view_metallic;

    glm::ivec2 viewport_size{1280, 720};
    Camera camera{glm::vec3{0, 0, 4}, glm::vec3{0}};

    Renderer(Shader shadow, Shader skybox, unsigned int skybox_texture);
    ~Renderer();

    size_t register_mesh(const Mesh &mesh);
    void render_mesh_instance(unsigned int vao, const MeshInstance &mesh);
    void render(std::vector<RenderData> &m);

    std::variant<uint, Error> register_texture(const Texture &texture);
};

} // namespace engine