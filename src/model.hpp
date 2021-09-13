#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "constants.hpp"

namespace engine
{

struct Sampler
{
    int magnify_filter = GL_LINEAR;
    int minify_filter = GL_LINEAR;
    int wrap_s = GL_REPEAT;
    int wrap_t = GL_REPEAT;
    //    int wrap_r;
    bool use_mipmap = false;
    bool is_srgb = false;
};

struct Texture
{
    int width;
    int height;
    int component_count;

    Sampler sampler;

    std::unique_ptr<uint8_t, void (*)(void *)> data;

    Texture(int width, int height, int component_count,
            std::unique_ptr<uint8_t, void (*)(void *)> data, Sampler sampler);

    static std::optional<Texture> from_file(std::filesystem::path path,
                                            Sampler sampler);
    static std::optional<Texture> from_memory(uint8_t *data, int length,
                                              Sampler sampler);
};

struct Mesh
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> tex_coords;
    std::vector<glm::vec4> tangents;
    std::vector<std::uint32_t> indices;
};

struct Material
{
    std::optional<Texture> normal;
    std::optional<Texture> emissive;
    std::optional<Texture> occlusion;
    std::optional<Texture> base_color;
    std::optional<Texture> metallic_roughness;
    std::optional<Texture> emisive;

    glm::vec4 base_color_factor{1.f};
    float metallic_factor;
    float roughness_factor;
};

struct Model
{
    std::unique_ptr<Mesh> mesh;
    Material material;
    glm::mat4 transform;
};

std::vector<Model> load_gltf(const std::filesystem::path &path);

} // namespace engine