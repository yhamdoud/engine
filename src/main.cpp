#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <streambuf>
#include <string>
#include <string_view>
#include <unordered_map>

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "glm/gtx/string_cast.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include "model.hpp"

using std::array;
using std::optional;
using std::string;
using std::string_view;
using std::unordered_map;
using std::filesystem::path;

using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

using namespace engine;

unsigned int window_width = 640;
unsigned int window_height = 480;

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << std::endl;
    abort();
}

void gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                         GLsizei length, GLchar const *message,
                         void const *user_param)
{
    const string source_string{[source]()
                               {
                                   switch (source)
                                   {
                                   case GL_DEBUG_SOURCE_API:
                                       return "API";
                                   case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
                                       return "Window system";
                                   case GL_DEBUG_SOURCE_SHADER_COMPILER:
                                       return "Shader compiler";
                                   case GL_DEBUG_SOURCE_THIRD_PARTY:
                                       return "Third party";
                                   case GL_DEBUG_SOURCE_APPLICATION:
                                       return "Application";
                                   case GL_DEBUG_SOURCE_OTHER:
                                       return "Other";
                                   }
                               }()};

    const string type_string{[type]()
                             {
                                 switch (type)
                                 {
                                 case GL_DEBUG_TYPE_ERROR:
                                     return "Error";
                                 case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
                                     return "Deprecated behavior";
                                 case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                                     return "Undefined behavior";
                                 case GL_DEBUG_TYPE_PORTABILITY:
                                     return "Portability";
                                 case GL_DEBUG_TYPE_PERFORMANCE:
                                     return "Performance";
                                 case GL_DEBUG_TYPE_MARKER:
                                     return "Marker";
                                 case GL_DEBUG_TYPE_OTHER:
                                     return "Other";
                                 }
                             }()};

    const string severity_string{[severity]()
                                 {
                                     switch (severity)
                                     {
                                     case GL_DEBUG_SEVERITY_NOTIFICATION:
                                         return "Notification";
                                     case GL_DEBUG_SEVERITY_LOW:
                                         return "Low";
                                     case GL_DEBUG_SEVERITY_MEDIUM:
                                         return "Medium";
                                     case GL_DEBUG_SEVERITY_HIGH:
                                         return "High";
                                     }
                                 }()};

    std::cout << "(GL message) Type: " << type_string
              << ", Severity: " << severity_string
              << ", Source: " << source_string << ", Message: " << message
              << std::endl;
}

static void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    window_height = width;
    window_height = height;
    glViewport(0, 0, window_width, window_height);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

std::string from_file(const path &path)
{
    std::ifstream file{path};
    return string{std::istreambuf_iterator<char>(file),
                  std::istreambuf_iterator<char>()};
}

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

unsigned int create_program(unsigned int vert_shader, unsigned int frag_shader)
{
    unsigned int program = glCreateProgram();
    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
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

struct Uniform
{
    int location;
    int count;
};

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

void set_uniform(unsigned int program, Uniform uniform, const mat4 &value)
{
    glProgramUniformMatrix4fv(program, uniform.location, uniform.count, false,
                              glm::value_ptr(value));
}

void set_uniform(unsigned int program, Uniform uniform, const mat3 &value)
{
    glProgramUniformMatrix3fv(program, uniform.location, uniform.count, false,
                              glm::value_ptr(value));
}

void set_uniform(unsigned int program, Uniform uniform, const vec3 &value)
{
    glProgramUniform3fv(program, uniform.location, uniform.count,
                        glm::value_ptr(value));
}

int main()
{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        std::cerr << "GLFW initialization failed." << std::endl;
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(window_width, window_height,
                                          "triangle", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Window creation failed." << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "glad error" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Enable error callback.
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_message_callback, nullptr);

    glEnable(GL_DEPTH_TEST);

    unsigned int vert_shader = create_shader(
        from_file(path{"../resources/shaders/shader.vs"}), GL_VERTEX_SHADER);
    unsigned int frag_shader = create_shader(
        from_file(path{"../resources/shaders/shader.fs"}), GL_FRAGMENT_SHADER);
    unsigned int program = create_program(vert_shader, frag_shader);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    auto mesh = Mesh::from_gtlf("../models/duck.glb")[0];

    unsigned int vbo;
    glCreateBuffers(1, &vbo);

    int size_indices = mesh.indices.size() * sizeof(uint32_t);
    int size_positions = mesh.positions.size() * sizeof(vec3);
    int size_normals = mesh.normals.size() * sizeof(vec3);
    int size_tex_coords = mesh.tex_coords.size() * sizeof(vec2);

    // TODO: Investigate if interleaved storage is more efficient than
    // sequential storage for vertex data.
    glNamedBufferStorage(vbo, size_indices + size_positions + size_normals,
                         nullptr, GL_DYNAMIC_STORAGE_BIT);

    int offset = 0;
    glNamedBufferSubData(vbo, offset, size_indices, mesh.indices.data());
    offset += size_indices;
    glNamedBufferSubData(vbo, offset, size_positions, mesh.positions.data());
    offset += size_positions;
    glNamedBufferSubData(vbo, offset, size_normals, mesh.normals.data());
    offset += size_normals;
    // glNamedBufferSubData(vbo, offset, size_tex_coords,
    // mesh.tex_coords.data());

    unsigned int vao;
    glCreateVertexArrays(1, &vao);

    unsigned int binding_positions = 0;
    unsigned int binding_normals = 1;

    glVertexArrayElementBuffer(vao, vbo);
    glVertexArrayVertexBuffer(vao, binding_positions, vbo, size_indices,
                              sizeof(vec3));
    glVertexArrayVertexBuffer(vao, binding_normals, vbo,
                              size_indices + size_positions, sizeof(vec3));

    int attrib_positions = 0;
    int attrib_normals = 1;
    int attrib_tex_coords = 2;

    glEnableVertexArrayAttrib(vao, attrib_positions);
    glEnableVertexArrayAttrib(vao, attrib_normals);

    glVertexArrayAttribFormat(vao, attrib_positions, 3, GL_FLOAT, false, 0);
    glVertexArrayAttribBinding(vao, attrib_positions, binding_positions);

    glVertexArrayAttribFormat(vao, attrib_normals, 3, GL_FLOAT, false, 0);
    glVertexArrayAttribBinding(vao, attrib_normals, binding_normals);

    glBindVertexArray(vao);
    glUseProgram(program);

    glClearColor(0.f, 0.f, 0.f, 1.0f);

    const mat4 view = glm::lookAt(vec3{0, 0, -3}, vec3{0.f}, vec3{0, 1, 0});

    Uniform uniform_mvp, uniform_normal_mat, uniform_model_view,
        uniform_light_pos;

    if (auto uniform_map = parse_uniforms(program))
    {
        try
        {
            uniform_mvp = uniform_map->at("u_mvp");
            uniform_normal_mat = uniform_map->at("u_normal_mat");
            uniform_model_view = uniform_map->at("u_model_view");
            uniform_light_pos = uniform_map->at("u_light_pos");
        }
        catch (std::exception e)
        {
            std::cerr << "Missing uniform in shader" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        std::cerr << "No uniforms in shader" << std::endl;
        exit(EXIT_FAILURE);
    }

    const vec3 up = vec3{0, 1, 0};

    double last_time = glfwGetTime();

    mat4 model = glm::scale(mat4{1}, vec3{0.01f});

    float turn_speed = glm::radians(45.f);

    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Timestep.
        float time = glfwGetTime();
        float delta_time = time - last_time;
        last_time = time;

        model = glm::rotate(delta_time * turn_speed, up) * model;

        const mat4 view = glm::lookAt(vec3{0, 0, 2}, vec3{0.f}, up);

        mat4 perspective =
            glm::perspective(glm::radians(90.f),
                             static_cast<float>(window_width) /
                                 static_cast<float>(window_height),
                             0.1f, 100.f);

        const auto model_view = view * model;
        const auto mvp = perspective * model_view;

        vec4 light_pos = view * vec4{10, 10, 10, 1};

        set_uniform(program, uniform_mvp, mvp);
        set_uniform(program, uniform_normal_mat,
                    glm::inverseTranspose(mat3{model_view}));
        set_uniform(program, uniform_model_view, view * model);
        set_uniform(program, uniform_light_pos, vec3(light_pos) / light_pos.w);

        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(program);

    glfwTerminate();

    return EXIT_SUCCESS;
}
