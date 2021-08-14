#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <glad/glad.h>

using std::array;
using std::string;

constexpr unsigned int window_width = 640;
constexpr unsigned int window_height = 480;

static const char *vert_source = "#version 460 core\n"
                                 "layout (location = 0) in vec3 aPos;"
                                 "layout (location = 1) in vec3 aCol;"
                                 "out vec3 vCol;"
                                 "void main()"
                                 "{"
                                 "   vCol = aCol;"
                                 "   gl_Position = vec4(aPos, 1.);"
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
    glViewport(0, 0, width, height);
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
        string log;
        log.reserve(length);
        glGetShaderInfoLog(shader, length, nullptr, &log[0]);
        std::cerr << log << std::endl;
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
        string log;
        log.reserve(length);
        glGetProgramInfoLog(program, length, nullptr, &log[0]);
        std::cerr << log << std::endl;
    }

    return program;
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

    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

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
