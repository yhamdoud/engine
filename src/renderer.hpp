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
    unsigned int shader_phong;
    std::unordered_map<std::string, Uniform> phong_uniforms;

    glm::ivec2 shadow_map_size{1024, 1024};
    unsigned int fbo_shadow;
    unsigned int texture_shadow;
    unsigned int shader_shadow;
    std::unordered_map<std::string, Uniform> shadow_uniforms;

    unsigned int vao_skybox;
    unsigned int shader_skybox;
    unsigned int texture_skybox;
    std::unordered_map<std::string, Uniform> skybox_uniforms;

  public:
    glm::ivec2 viewport_size{1280, 720};
    Camera camera{glm::vec3{0, 0, 4}, glm::vec3{0}};

    Renderer(unsigned int shader, unsigned int shadow, unsigned int skybox,
             unsigned int shadow_texture);
    ~Renderer();

    void register_entity(const Entity &e);
    static void render_data(unsigned int vao, RenderData data);
    void render();
};

} // namespace engine