#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "constants.hpp"

namespace engine
{

struct Texture
{
    int width;
    int height;
    int component_count;
    std::unique_ptr<uint8_t, void (*)(void *)> data;

    Texture(int width, int height, int component_count,
            std::unique_ptr<uint8_t, void (*)(void *)> data);

    static std::optional<Texture> from_file(std::filesystem::path path);
    static std::optional<Texture> from_memory(uint8_t *data, int length);
};

struct Mesh
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> tex_coords;
    std::vector<std::uint32_t> indices;
};

struct Model
{
    std::unique_ptr<Mesh> mesh;
    std::unique_ptr<Texture> base_color;
    glm::mat4 transform;
};

std::vector<Model> load_gltf(const std::filesystem::path &path);

} // namespace engine