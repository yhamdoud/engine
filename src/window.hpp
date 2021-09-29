#pragma once

#include <functional>
#include <optional>
#include <variant>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "constants.hpp"
#include "error.hpp"

namespace engine
{

class Window
{
    glm::ivec2 size;

  public:
    // TODO: Wrap this behind an interface completely, eventually.
    GLFWwindow *impl;

    Window(glm::ivec2 size, const char *title);
    ~Window();

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window(Window &&) = delete;
    Window &operator=(Window &&) = delete;

    void run(const std::function<void()> &main_loop);
    glm::vec2 get_cursor_position();

    static bool load_gl();
};

std::optional<GLFWwindow *> init_glfw(uint width, uint height);

static void glfw_error_callback(int error, const char *description);

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods);

static void scroll_callback(GLFWwindow *window, double x_offset,
                            double y_offset);

static void framebuffer_size_callback(GLFWwindow *window, int width,
                                      int height);

} // namespace engine