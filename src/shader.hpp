#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
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
    static std::optional<Shader>
    from_comp_path(const std::filesystem::path &path);
    static std::optional<Shader>
    from_stages(const std::initializer_list<uint> &stages);

    uint get_id() const;

    void set(const std::string &name, const glm::mat4 &value) const;
    void set(const std::string &name, const std::span<glm::mat4> values) const;
    void set(const std::string &name, const glm::mat3 &value) const;
    void set(const std::string &name, const glm::vec2 &value) const;
    void set(const std::string &name, const glm::vec3 &value) const;
    void set(const std::string &name, const glm::ivec3 &value) const;
    void set(const std::string &name, const std::span<glm::vec3> values) const;
    void set(const std::string &name, float value) const;
    void set(const std::string &name, std::span<float> value) const;
    void set(const std::string &name, int value) const;
    void set(const std::string &name, uint value) const;
    void set(const std::string &name, bool value) const;
};

GLuint upload_cube_map(const std::array<std::filesystem::path, 6> &paths);
