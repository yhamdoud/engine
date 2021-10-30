#define STB_IMAGE_IMPLEMENTATION

#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <regex>
#include <string_view>
#include <utility>
#include <variant>

#include <glad/glad.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <stb_image.h>

#include "constants.hpp"
#include "logger.hpp"
#include "primitives.hpp"
#include "shader.hpp"
#include "utils.hpp"

using std::filesystem::path;

using namespace engine;
using namespace std;

// https://community.khronos.org/t/include-in-glsl/59203/6
static std::string process_includes(const std::string &source,
                                    const path &filename, int level)
{
    if (level > 16)
        logger.error("Cyclic includes");

    regex re("^[ ]*#[ ]*include[ ]+[\"<](.*)[\">].*");

    stringstream input;
    stringstream output;
    input << source;

    size_t line_number = 1;
    smatch matches;

    string line;
    while (std::getline(input, line))
    {
        if (regex_search(line, matches, re))
        {
            std::string include_file = matches[1];
            std::string include_string;

            auto path = shaders_path;
            path.concat(include_file);

            if (filesystem::exists(path))
            {
                include_string = utils::from_file(path);
            }
            else
            {
                logger.error("Cannot open include file: {}", include_file);
            }

            output << process_includes(include_string, include_file, level + 1)
                   << endl;
        }
        else
        {
            // if (line_number != 1)
            //     output << fmt::format("#line {} \"{}\"\n{}", line_number,
            //                           filename.string(), line)
            //            << endl;
            // else
            output << line << endl;
        }

        ++line_number;
    }

    return output.str();
}

uint Shader::compile_shader_stage(string source, path path,
                                  const string &defines, GLenum stage)
{
    uint shader = glCreateShader(stage);

    auto version_start = source.find("#version");
    auto version_stop = source.find('\n', version_start) + 1;

    string default_defines = "#define ENGINE_DEFINES\n";
    default_defines.append(defines);

    source.insert(version_stop, default_defines);

    auto processed = process_includes(source, path, 0);

    auto c_str = processed.c_str();

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
        logger.error("Shader {} compilation failure:\n{}", path.string(),
                     log.get());
        return invalid_shader_id;
    }

    return shader;
}

optional<Shader> Shader::from_paths(const ShaderPaths &p,
                                    const ShaderDefines &d)
{
    uint vert = invalid_shader_id, geom = invalid_shader_id,
         frag = invalid_shader_id;

    if (std::filesystem::exists(p.vert))
    {
        vert = compile_shader_stage(utils::from_file(p.vert), p.vert, d.vert,
                                    GL_VERTEX_SHADER);
        if (vert == invalid_shader_id)
            return std::nullopt;
    }
    else
    {
        logger.error("Vertex shader not found at path: {}", p.vert.string());
        return std::nullopt;
    }

    if (!p.geom.empty())
    {
        if (std::filesystem::exists(p.geom))
        {
            geom = compile_shader_stage(utils::from_file(p.geom), p.geom,
                                        d.geom, GL_GEOMETRY_SHADER);
            if (geom == invalid_shader_id)
                return std::nullopt;
        }
        else
        {
            logger.error("Geometry shader not found at path: {}",
                         p.geom.string());
            return std::nullopt;
        }
    }

    if (!p.frag.empty())
    {
        if (std::filesystem::exists(p.frag))
        {
            frag = compile_shader_stage(utils::from_file(p.frag), p.frag,
                                        d.frag, GL_FRAGMENT_SHADER);
            if (frag == invalid_shader_id)
                return std::nullopt;
        }
        else
        {
            logger.error("Fragment shader not found at path: {}",
                         p.frag.string());
            return std::nullopt;
        }
    }

    if (p.geom.empty() && !p.frag.empty())
        return Shader::from_stages({vert, frag});

    if (!p.geom.empty() && p.frag.empty())
        return Shader::from_stages({vert, geom});

    if (!p.geom.empty() && !p.frag.empty())
        return Shader::from_stages({vert, geom, frag});

    return Shader::from_stages({vert});
}

std::optional<Shader> Shader::from_comp_path(const path &path,
                                             const string &defines)
{
    uint comp;

    if (std::filesystem::exists(path))
    {
        comp = compile_shader_stage(utils::from_file(path), path, defines,
                                    GL_COMPUTE_SHADER);
        if (comp == invalid_shader_id)
            return std::nullopt;
    }
    else
    {
        logger.error("Compute shader not found at path: {}", path.string());
        return std::nullopt;
    }

    return Shader::from_stages({comp});
}

Shader::Shader(uint id, UniformMap uniforms)
    : id{id}, uniforms{std::move(uniforms)}
{
}

Shader::Shader() : id{invalid_shader_id}, uniforms{UniformMap{}} {}

optional<Shader> Shader::from_stages(const std::initializer_list<uint> &stages)
{
    unsigned int program = glCreateProgram();

    for (const auto &stage : stages)
        glAttachShader(program, stage);

    glLinkProgram(program);

    int is_linked;
    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (!is_linked)
    {
        int length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        auto log = std::make_unique<char[]>(length);
        glGetProgramInfoLog(program, length, nullptr, log.get());
        logger.error("Shader linking failure:\n{}", log.get());
        return std::nullopt;
    }

    auto uniforms = parse_uniforms(program);

    for (const auto &stage : stages)
        glDeleteShader(stage);

    return std::make_optional<Shader>(Shader{program, uniforms});
}

void Shader::set(const std::string &name, const glm::mat4 &value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniformMatrix4fv(id, uniform.location, uniform.count, false,
                              glm::value_ptr(value));
}

void Shader::set(const std::string &name,
                 const std::span<glm::mat4> values) const
{
    auto uniform = uniforms.at(name);
    glProgramUniformMatrix4fv(id, uniform.location, uniform.count, false,
                              reinterpret_cast<float *>(values.data()));
}

void Shader::set(const std::string &name, const glm::mat3 &value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniformMatrix3fv(id, uniform.location, uniform.count, false,
                              glm::value_ptr(value));
}

void Shader::set(const std::string &name, const glm::vec2 &value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform2fv(id, uniform.location, uniform.count,
                        glm::value_ptr(value));
}

void Shader::set(const std::string &name, const glm::ivec2 &value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform2iv(id, uniform.location, uniform.count,
                        glm::value_ptr(value));
}

void Shader::set(const std::string &name, const glm::vec3 &value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform3fv(id, uniform.location, uniform.count,
                        glm::value_ptr(value));
}

void Shader::set(const std::string &name,
                 const std::span<glm::vec3> values) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform3fv(id, uniform.location, uniform.count,
                        reinterpret_cast<float *>(values.data()));
}

void Shader::set(const std::string &name, const glm::ivec3 &value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform3iv(id, uniform.location, uniform.count,
                        glm::value_ptr(value));
}

void Shader::set(const std::string &name, float value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform1f(id, uniform.location, value);
}

void Shader::set(const std::string &name, std::span<float> value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform1fv(id, uniform.location, uniform.count, value.data());
}

void Shader::set(const std::string &name, int value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform1i(id, uniform.location, value);
}

void Shader::set(const std::string &name, uint value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform1ui(id, uniform.location, value);
}

void Shader::set(const std::string &name, bool value) const
{
    auto uniform = uniforms.at(name);
    glProgramUniform1i(id, uniform.location, value);
}

uint Shader::get_id() const { return id; }

UniformMap Shader::parse_uniforms(uint program)
{
    int count;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);

    if (count == 0)
        return UniformMap{};

    int max_length;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_length);

    auto name = std::make_unique<char[]>(max_length);

    UniformMap uniforms(count);

    int length, size;
    GLenum type;

    for (int i = 0; i < count; i++)
    {
        glGetActiveUniform(program, i, max_length, &length, &size, &type,
                           &name[0]);

        Uniform uniform{glGetUniformLocation(program, name.get()), size};
        uniforms.emplace(std::make_pair(string(name.get(), length), uniform));
    }

    return uniforms;
}

GLuint engine::upload_cube_map(const std::array<path, 6> &paths)
{
    unsigned int id;

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &id);

    GLenum format = GL_RGB;

    for (std::size_t face_idx = 0; face_idx < 6; face_idx++)
    {
        const auto &path = paths[face_idx];

        int width, height, channel_count;

        glm::uint8_t *data = stbi_load(path.string().c_str(), &width, &height,
                                       &channel_count, 0);

        if (face_idx == 0)
            glTextureStorage2D(id, 1, GL_RGBA8, width, height);

        if (data)
        {
            glTextureSubImage3D(id, 0, 0, 0, face_idx, width, height, 1, format,
                                GL_UNSIGNED_BYTE, data);

            free(data);
        }
        else
        {
            logger.error("Failed to load textures: {}", path.string());
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