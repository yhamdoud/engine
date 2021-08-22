#pragma once;

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <glad/glad.h>

#include <glm/glm.hpp>

struct Uniform {
    int location;
    int count;
};

void set_uniform(unsigned int program, Uniform uniform, const glm::mat4 &value);

void set_uniform(unsigned int program, Uniform uniform, const glm::mat3 &value);

void set_uniform(unsigned int program, Uniform uniform, const glm::vec3 &value);

struct ProgramSources {
    std::filesystem::path vert;
    std::filesystem::path geom;
    std::filesystem::path frag;
};

std::optional<std::unordered_map<std::string, Uniform>>
parse_uniforms(unsigned int program);

unsigned int create_shader(const std::string &source, GLenum type);

unsigned int create_shader(const std::filesystem::path &path, GLenum type);

unsigned int create_program(unsigned int vert, unsigned int geom,
                            unsigned int frag);

unsigned int create_program(const ProgramSources &srcs);

GLuint upload_cube_map(const std::array<std::filesystem::path, 6> &paths);
