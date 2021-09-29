#pragma once

#include "renderer/renderer.hpp"
#include "window.hpp"

namespace engine
{

class Editor
{
    GLFWwindow &window;
    Renderer &renderer;
    int bounce_count = 1;
    float distance = 1.f;

  public:
    Editor(GLFWwindow &window, Renderer &r);
    ~Editor();

    Editor(const Editor &) = delete;
    Editor(Editor &&) = delete;
    Editor &operator=(const Editor &) = delete;
    Editor &operator=(Editor &&) = delete;

    void draw();
};

} // namespace engine