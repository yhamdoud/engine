#pragma once

#include <filesystem>

namespace engine
{
using uint = unsigned int;

const unsigned int invalid_texture_id = 0;
const unsigned int invalid_shader_id = 0;

const std::filesystem::path resources_path{"../resources"};
const std::filesystem::path textures_path{resources_path / "textures"};
const std::filesystem::path shaders_path{resources_path / "shaders"};
const std::filesystem::path models_path{resources_path / "models"};

} // namespace engine
