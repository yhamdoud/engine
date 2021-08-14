#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

using std::array;
using std::optional;
using std::string;
using std::unordered_map;

using glm::mat4;
using glm::vec3;
using glm::vec4;

unsigned int window_width = 640;
unsigned int window_height = 480;

static const char *vert_source = "#version 460 core\n"
                                 "layout (location = 0) in vec3 aPos;"
                                 "layout (location = 1) in vec3 aCol;"
                                 "out vec3 vCol;"
                                 "uniform mat4 u_mvp;"
                                 "void main()"
                                 "{"
                                 "   vCol = aCol;"
                                 "   gl_Position = u_mvp * vec4(aPos, 1.);"
                                 "}";

static const char *frag_source = "#version 460 core\n"
                                 "in vec3 vCol;"
                                 "out vec4 fragColor;"
                                 "void main()"
                                 "{"
                                 "   fragColor = vec4(vCol, 1.0f);"
                                 "}";

static void error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << std::endl;
    abort();
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

unsigned int create_shader(const char *source, GLenum type)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
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

void set_uniform(unsigned int program, Uniform uniform, const vec3 &value)
{
    glProgramUniform3fv(program, uniform.location, uniform.count,
                        glm::value_ptr(value));
}

int main()
{
    glfwSetErrorCallback(error_callback);

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

    unsigned int vert_shader = create_shader(vert_source, GL_VERTEX_SHADER);
    unsigned int frag_shader = create_shader(frag_source, GL_FRAGMENT_SHADER);
    unsigned int program = create_program(vert_shader, frag_shader);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    array<float, 18> vertices{
        -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f,
        0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,  1.0f,
    };

    unsigned int vbo, vao;

    glCreateBuffers(1, &vbo);
    glNamedBufferData(vbo, sizeof(vertices), vertices.begin(), GL_STATIC_DRAW);

    glCreateVertexArrays(1, &vao);

    unsigned int binding_point = 0;
    glVertexArrayVertexBuffer(vao, binding_point, vbo, 0, 6 * sizeof(float));

    int pos_attrib_idx = 0, col_attrib_idx = 1;
    glEnableVertexArrayAttrib(vao, pos_attrib_idx);
    glEnableVertexArrayAttrib(vao, col_attrib_idx);

    glVertexArrayAttribFormat(vao, pos_attrib_idx, 3, GL_FLOAT, false, 0);
    glVertexArrayAttribFormat(vao, col_attrib_idx, 3, GL_FLOAT, false,
                              3 * sizeof(float));

    glVertexArrayAttribBinding(vao, pos_attrib_idx, binding_point);
    glVertexArrayAttribBinding(vao, col_attrib_idx, binding_point);

    glBindVertexArray(vao);
    glUseProgram(program);

    glClearColor(0.f, 0.f, 0.f, 1.0f);

    const mat4 model = mat4{1};
    const mat4 view = glm::lookAt(vec3{0, 0, -2}, vec3{0.f}, vec3{0, 1, 0});

    Uniform mvp_uniform;

    if (auto uniform_map = parse_uniforms(program))
    {
        mvp_uniform = uniform_map->at("u_mvp");
    }
    else
    {
        std::cerr << "Missing uniform in shader" << std::endl;
    }

    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        mat4 perspective =
            glm::perspective(glm::radians(90.f),
                             static_cast<float>(window_width) /
                                 static_cast<float>(window_height),
                             0.1f, 100.f);

        set_uniform(program, mvp_uniform, perspective * view * model);

        glDrawArrays(GL_TRIANGLES, 0, 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(program);

    glfwTerminate();

    return EXIT_SUCCESS;
}
