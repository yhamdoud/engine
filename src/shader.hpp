#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <glad/glad.h>

#include <glm/glm.hpp>

struct Uniform
{
    int location;
    int count;
};

struct ShaderPaths
{
    std::filesystem::path vert;
    std::filesystem::path geom;
    std::filesystem::path frag;
};

using UniformMap = std::unordered_map<std::string, Uniform>;

class Shader
{
    uint id;
    static UniformMap parse_uniforms(uint program);
    static uint compile_shader_stage(const std::string &source, GLenum stage);

  public:
    UniformMap uniforms;

    Shader();
    Shader(uint program, UniformMap uniforms);
    static std::optional<Shader> from_paths(const ShaderPaths &p);
    static std::optional<Shader> from_stages(uint vert, uint geom, uint frag);

    uint get_id() const;

    void set(const std::string &name, const glm::mat4 &value);
    void set(const std::string &name, const glm::mat3 &value);
    void set(const std::string &name, const glm::vec3 &value);
    void set(const std::string &name, int value);
    void set(const std::string &name, bool value);
};

GLuint upload_cube_map(const std::array<std::filesystem::path, 6> &paths);
