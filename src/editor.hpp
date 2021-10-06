#pragma once

#include "renderer/renderer.hpp"
#include "window.hpp"

namespace engine
{

class Editor
{
    Window &window;
    Renderer &renderer;
    int bounce_count = 1;
    float distance = 1.f;
    glm::vec3 light_dir{0.f, -1.f, 1.f};

    void draw_profiler();
    void draw_renderer_menu();

  public:
    Editor(Window &window, Renderer &renderer);
    ~Editor();

    Editor(const Editor &) = delete;
    Editor(Editor &&) = delete;
    Editor &operator=(const Editor &) = delete;
    Editor &operator=(Editor &&) = delete;

    void draw();
};

} // namespace engine