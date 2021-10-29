#include <iostream>

#include <glad/glad.h>

#include "logger.hpp"
#include "profiler.hpp"
#include "renderer/renderer.hpp"
#include "window.hpp"

using namespace std;
using namespace glm;
using namespace engine;

bool Window::init_glfw()
{
    if (!glfwInit())
        return false;

    glfwSetErrorCallback(
        [](int error, const char *description)
        { logger.error("GLFW ({}): {}", error, description); });

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    return true;
}

bool Window::init_gl()
{
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

    // Enable error callback.
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(
        [](GLenum source, GLenum type, GLuint id, GLenum severity,
           GLsizei length, GLchar const *message, void const *user_param)
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

            logger.log(log_type, "OpenGL {0} {1}: {3}", type_string,
                       source_string, id, message);
        },
        nullptr);

    // Disable shader compiler spam.
    glDebugMessageControl(GL_DEBUG_SOURCE_SHADER_COMPILER, GL_DEBUG_TYPE_OTHER,
                          GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, false);

    return true;
}

Window::Window(ivec2 size, const char *title) : size(size)
{
    impl = glfwCreateWindow(size.x, size.y, title, nullptr, nullptr);
    glfwSetWindowUserPointer(impl, this);

    if (impl == nullptr)
    {
        logger.error("Window creation failed.");
        abort();
    }

    glfwSetKeyCallback(
        impl,
        [](GLFWwindow *w, int key, int scancode, int action, int mods)
        {
            auto *usr = reinterpret_cast<Window *>(glfwGetWindowUserPointer(w));

            if (action == GLFW_PRESS)
                usr->on_key_press(key, scancode, mods);
            else
                // TODO:
                ;
        });

    glfwSetScrollCallback(impl,
                          [](GLFWwindow *w, double offset_x, double offset_y)
                          {
                              reinterpret_cast<Window *>(
                                  glfwGetWindowUserPointer(w))
                                  ->on_mouse_scroll(offset_x, offset_y);
                          });

    glfwSetMouseButtonCallback(
        impl,
        [](GLFWwindow *w, int button, int action, int mods)
        {
            reinterpret_cast<Window *>(glfwGetWindowUserPointer(w))
                ->on_mouse_button_press(button, action, mods);
        });

    glfwMakeContextCurrent(impl);
}

Window::~Window()
{
    close();
    glfwDestroyWindow(impl);
    glfwTerminate();
}

void Window::close() { glfwSetWindowShouldClose(impl, GLFW_TRUE); }

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

void Window::resize(ivec2 resolution)
{
    size = resolution;
    glfwSetWindowSize(impl, resolution.x, resolution.y);
}

void Window::set_full_screen(bool enable)
{
    is_full_screen = enable;

    if (enable)
    {
        glfwSetWindowMonitor(impl, glfwGetPrimaryMonitor(), 0, 0, size.x,
                             size.y, GLFW_DONT_CARE);
    }
    else
    {
        glfwSetWindowMonitor(impl, nullptr, 0, 0, size.x, size.y,
                             GLFW_DONT_CARE);
    }
}

void Window::add_key_callback(int key, const KeyCallback &callback)
{
    key_callbacks[key].push_back(callback);
}

void Window::add_mouse_button_callback(int button,
                                       const MouseButtonCallback &callback)
{
    mouse_button_callbacks[button].push_back(callback);
}

void Window::add_mouse_scroll_callback(const MouseScrollCallback &callback)
{
    mouse_scroll_callbacks.push_back(callback);
}

void Window::on_mouse_button_press(int button, int action, int mods)
{
    for (const auto &callback : mouse_button_callbacks[button])
        callback(action, mods);
}

void Window::on_mouse_scroll(double offset_x, double offset_y)
{
    for (const auto &callback : mouse_scroll_callbacks)
        callback(offset_x, offset_y);
}

void Window::on_key_press(int key, int action, int mods)
{
    for (const auto &callback : key_callbacks[key])
        callback(action, mods);
}
