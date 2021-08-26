#pragma once

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
};

struct RenderData
{
    Entity::Flags flags;
    size_t mesh_index;
    glm::mat4 model;
    Shader &shader;
};

struct Light
{
    glm::vec3 position;
};

class Renderer
{
    Light light;

    std::vector<MeshInstance> mesh_instances;

    // FIXME:
    unsigned int vao_entities;
    unsigned int vao_skybox;

    Shader shadow_shader;
    glm::ivec2 shadow_map_size{1024, 1024};
    unsigned int texture_shadow;
    unsigned int fbo_shadow;

    Shader skybox_shader;
    unsigned int texture_skybox;

  public:
    glm::ivec2 viewport_size{1280, 720};
    Camera camera{glm::vec3{0, 0, 4}, glm::vec3{0}};

    Renderer(Shader shadow, Shader skybox, unsigned int skybox_texture);
    ~Renderer();

    size_t register_mesh(const Mesh &mesh);
    void render_mesh_instance(unsigned int vao, const MeshInstance &mesh);
    void render(std::vector<RenderData> &m);
};

} // namespace engine