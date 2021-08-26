#pragma once

#include <optional>
#include <variant>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "constants.hpp"
#include "error.hpp"

namespace engine
{

struct Window
{
    uint width;
    uint height;
};

extern Window window_data;

std::optional<GLFWwindow *> init_glfw(uint width, uint height);

static void glfw_error_callback(int error, const char *description);

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods);

static void scroll_callback(GLFWwindow *window, double x_offset,
                            double y_offset);

static void framebuffer_size_callback(GLFWwindow *window, int width,
                                      int height);

glm::vec2 get_cursor_position(GLFWwindow *window);

} // namespace engine