#define STB_IMAGE_IMPLEMENTATION

#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>

#include <glad/glad.h>
#include <glm/ext.hpp>

#include <glm/glm.hpp>
#include <stb_image.h>

#include <Tracy.hpp>

#include "constants.hpp"
#include "shader.hpp"
#include "utils.hpp"

using std::optional;
using std::string;
using std::unordered_map;
using std::filesystem::path;

using namespace engine;

unsigned int create_shader(const string &source, GLenum type)
{

    unsigned int shader = glCreateShader(type);

    auto c_str = source.c_str();
    glShaderSource(shader, 1, &c_str, nullptr);
    glCompileShader(shader);

    int is_compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
    if (!is_compiled)
    {
        int length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        auto log = std::make_unique<char[]>(length);
        glGetShaderInfoLog(shader, length, nullptr, log.get());
        std::cerr << "Shader failure: " << log << std::endl;
    }

    return shader;
}

unsigned int create_shader(const path &path, GLenum type)
{
    if (!std::filesystem::exists(path))
    {
        std::cerr << "Shader not found: " << path << std::endl;
        return 0;
    }

    return create_shader(utils::from_file(path), type);
}

unsigned int create_program(unsigned int vert, unsigned int geom,
                            unsigned int frag)
{
    unsigned int program = glCreateProgram();

    glAttachShader(program, vert);

    if (geom != 0)
        glAttachShader(program, geom);

    if (frag != 0)
        glAttachShader(program, frag);

    glLinkProgram(program);

    int is_linked;
    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (!is_linked)
    {
        int length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        auto log = std::make_unique<char[]>(length);
        glGetProgramInfoLog(program, length, nullptr, log.get());
        std::cerr << "Shader program failure: " << log << std::endl;
    }

    return program;
}

unsigned int create_program(const ProgramSources &srcs)
{
    auto vert = create_shader(srcs.vert, GL_VERTEX_SHADER);
    auto geom =
        !srcs.geom.empty() ? create_shader(srcs.geom, GL_GEOMETRY_SHADER) : 0;
    auto frag =
        !srcs.frag.empty() ? create_shader(srcs.frag, GL_FRAGMENT_SHADER) : 0;

    return create_program(vert, geom, frag);

    glDeleteShader(vert);
    glDeleteShader(geom);
    glDeleteShader(frag);
}

void set_uniform(unsigned int program, Uniform uniform, const glm::mat4 &value)
{
    glProgramUniformMatrix4fv(program, uniform.location, uniform.count, false,
                              glm::value_ptr(value));
}

void set_uniform(unsigned int program, Uniform uniform, const glm::mat3 &value)
{
    glProgramUniformMatrix3fv(program, uniform.location, uniform.count, false,
                              glm::value_ptr(value));
}

void set_uniform(unsigned int program, Uniform uniform, const glm::vec3 &value)
{
    glProgramUniform3fv(program, uniform.location, uniform.count,
                        glm::value_ptr(value));
}

optional<unordered_map<string, Uniform>> parse_uniforms(unsigned int program)
{
    int count;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);

    if (count == 0)
        return std::nullopt;

    int max_length;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_length);

    auto name = std::make_unique<char[]>(max_length);

    unordered_map<string, Uniform> name_uniform_map(count);

    int length, size;
    GLenum type;

    for (int i = 0; i < count; i++)
    {
        glGetActiveUniform(program, i, max_length, &length, &size, &type,
                           &name[0]);

        Uniform uniform{glGetUniformLocation(program, name.get()), size};
        name_uniform_map.emplace(
            std::make_pair(string(name.get(), length), uniform));
    }

    return std::make_optional(name_uniform_map);
}

GLuint upload_cube_map(const std::array<path, 6> &paths)
{
    unsigned int id;

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &id);

    GLenum format = GL_RGB;

    for (std::size_t face_idx = 0; face_idx < 6; face_idx++)
    {
        const auto &path = paths[face_idx];

        int width, height, channel_count;

        glm::uint8_t *data =
            stbi_load(path.c_str(), &width, &height, &channel_count, 0);

        if (face_idx == 0)
            glTextureStorage2D(id, 1, GL_RGBA8, width, height);

        if (data)
        {
            glad_glTextureSubImage3D(id, 0, 0, 0, face_idx, width, height, 1,
                                     format, GL_UNSIGNED_BYTE, data);

            free(data);
        }
        else
        {
            std::cerr << "Failed to load texture: " << path << std::endl;
            return invalid_texture_id;
        }
    }

    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return id;
}