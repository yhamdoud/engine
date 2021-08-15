#pragma once

#include <filesystem>
#include <vector>

#include "glm/glm.hpp"

namespace engine
{

class Mesh
{
  public:
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> tex_coords;
    std::vector<std::uint32_t> indices;

    static std::vector<Mesh> from_gtlf(const std::filesystem::path &path);
};

} // namespace engine