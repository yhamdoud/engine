#include <iostream>

#include "logger.hpp"
#include "renderer.hpp"
#include "window.hpp"

using namespace std;
using namespace glm;

engine::Window engine::window_data{0, 0};

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
    std::cerr << "GLFW error " << error << ": " << description << std::endl;
    abort();
}

void engine::framebuffer_size_callback(GLFWwindow *window, int width,
                                       int height)
{
    window_data.width = width;
    window_data.height = height;
}

optional<GLFWwindow *> engine::init_glfw(uint width, uint height)
{
    window_data.width = width;
    window_data.height = height;

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        logger.error("GLFW initialization failed.");
        return std::nullopt;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window =
        glfwCreateWindow(width, height, "triangle", nullptr, nullptr);

    if (window == nullptr)
    {
        logger.error("Window creation failed.");
        return std::nullopt;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwMakeContextCurrent(window);

    return make_optional<GLFWwindow *>(window);
}

vec2 engine::get_cursor_position(GLFWwindow *window)
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return {x, y};
}
