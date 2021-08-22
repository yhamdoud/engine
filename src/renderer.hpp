#pragma once

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.hpp"
#include "entity.hpp"
#include "shader.hpp"

namespace engine
{

struct RenderData
{
    Entity::Flags flags;
    GLuint buffer;
    glm::mat4 model;
    int primitive_count;
    int positions_offset;
    int normals_offset;
    Shader &shader;
};

struct Light
{
    glm::vec3 position;
};

class Renderer
{
    Light light;

    std::vector<RenderData> queue;

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

    void register_entity(const Entity &e);
    static void render_data(unsigned int vao, RenderData data);
    void render();
};

} // namespace engine