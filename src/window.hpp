#pragma once

#define GLFW_INCLUDE_NONE

#include <functional>
#include <map>
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
    using MouseButtonCallback = std::function<void(int, int)>;
    using MouseScrollCallback =
        std::function<void(double offset_x, double offset_y)>;
    using KeyCallback = std::function<void(int, int)>;

    glm::ivec2 size;
    std::unordered_map<int, std::vector<MouseButtonCallback>>
        mouse_button_callbacks{};
    std::vector<MouseScrollCallback> mouse_scroll_callbacks{};
    std::unordered_map<int, std::vector<KeyCallback>> key_callbacks{};

    void on_mouse_button_press(int button, int action, int mods);
    void on_mouse_scroll(double offset_x, double offset_y);
    void on_key_press(int key, int scancode, int mods);

  public:
    bool is_full_screen;

    GLFWwindow *impl;

    Window(glm::ivec2 size, const char *title);
    ~Window();

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window(Window &&) = delete;
    Window &operator=(Window &&) = delete;

    void close();
    void run(const std::function<void()> &main_loop);
    void resize(glm::ivec2 resolution);
    void set_full_screen(bool enable);

    glm::vec2 get_cursor_position();

    void add_key_callback(int key, const KeyCallback &callback);
    void add_mouse_button_callback(int button,
                                   const MouseButtonCallback &callback);
    void add_mouse_scroll_callback(const MouseScrollCallback &callback);

    static bool init_glfw();
    static bool init_gl();
};

} // namespace engine