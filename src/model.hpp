#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <cgltf.h>
#include <glad/glad.h>
#include <gli/gli.hpp>
#include <glm/glm.hpp>

#include "constants.hpp"
#include "gli/texture.hpp"

namespace engine
{

enum class ImportError
{
    invalid_path,
    missing_texture,
};

struct Sampler
{
    int magnify_filter = GL_LINEAR;
    int minify_filter = GL_LINEAR;
    int wrap_s = GL_REPEAT;
    int wrap_t = GL_REPEAT;
    //    int wrap_r;
    bool use_mipmap = true;
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

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 tex_coords;
    glm::vec4 tangent;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

enum class AlphaMode
{
    opaque,
    mask,
    blend,
};

using CompressedTexture = gli::texture;

struct Material
{
    uint normal = invalid_texture_id;
    uint emissive = invalid_texture_id;
    uint occlusion = invalid_texture_id;
    uint base_color = invalid_texture_id;
    uint metallic_roughness = invalid_texture_id;
    uint emisive = invalid_texture_id;

    glm::vec4 base_color_factor{1.f};
    float metallic_factor = 0.f;
    float roughness_factor = 0.f;

    AlphaMode alpha_mode = AlphaMode::opaque;
    float alpha_cutoff;
};

} // namespace engine