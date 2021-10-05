#define GLFW_INCLUDE_NONE

#include <iostream>

#include <glad/glad.h>

#include "logger.hpp"
#include "profiler.hpp"
#include "renderer/renderer.hpp"
#include "window.hpp"

using namespace std;
using namespace glm;
using namespace engine;

static void gl_message_callback(GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length,
                                GLchar const *message, void const *user_param)
{
    const string source_string{[&source]
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
                                       return "";
                                   }
                               }()};

    const string type_string{[&type]
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
                                     return "";
                                 }
                             }()};

    const auto log_type = [&severity]
    {
        switch (severity)
        {
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return LogType::info;
        case GL_DEBUG_SEVERITY_LOW:
            return LogType::warning;
        case GL_DEBUG_SEVERITY_MEDIUM:
        case GL_DEBUG_SEVERITY_HIGH:
            return LogType::error;
        }
    }();

    logger.log(log_type, "OpenGL {0} {1}: {3}", type_string, source_string, id,
               message);
}

Window::Window(ivec2 size, const char *title) : size(size)
{
    if (!glfwInit())
    {
        logger.error("GLFW initialization failed.");
        abort();
    }

    glfwSetErrorCallback(glfw_error_callback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    impl = glfwCreateWindow(size.x, size.y, title, nullptr, nullptr);

    if (impl == nullptr)
    {
        logger.error("Window creation failed.");
        abort();
    }

    glfwSetFramebufferSizeCallback(impl, framebuffer_size_callback);
    glfwSetKeyCallback(impl, key_callback);
    glfwSetScrollCallback(impl, scroll_callback);

    glfwMakeContextCurrent(impl);
}

Window::~Window()
{
    glfwDestroyWindow(impl);
    glfwTerminate();
}

bool Window::load_gl()
{
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

    // Enable error callback.
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_message_callback, nullptr);

    return true;
}

void Window::run(const function<void()> &main_loop)
{
    ZoneScoped;

    profiler_init();

    TracyGpuContext;

    while (!glfwWindowShouldClose(impl))
    {
        main_loop();

        glfwSwapBuffers(impl);
        glfwPollEvents();

        profiler_collect();

        TracyGpuCollect;
        FrameMark
    }
}

vec2 Window::get_cursor_position()
{
    double x, y;
    glfwGetCursorPos(impl, &x, &y);
    return vec2{x, y};
}

void engine::scroll_callback(GLFWwindow *window, double x_offset,
                             double y_offset)
{
    auto *renderer =
        reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));

    renderer->camera.zoom(y_offset);
}

void engine::key_callback(GLFWwindow *window, int key, int scancode, int action,
                          int mods)
{
    auto *renderer =
        reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    else if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        renderer->camera.reset();
    }
}
void engine::glfw_error_callback(int error, const char *description)
{
    logger.error("GLFW ({}): {}", error, description);
    abort();
}

void engine::framebuffer_size_callback(GLFWwindow *window, int width,
                                       int height)
{
    //    window_data.width = width;
    //    window_data.height = height;
}
