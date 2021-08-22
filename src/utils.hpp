#pragma once

#include <filesystem>
#include <string>

namespace utils
{

std::string from_file(const std::filesystem::path &path);

}; // namespace utils