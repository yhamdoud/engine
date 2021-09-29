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