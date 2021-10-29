#pragma once

#include "glm/fwd.hpp"
#include "renderer/renderer.hpp"
#include "window.hpp"

namespace engine
{

struct Gizmo
{
    bool do_snap = false;
    std::array<float, 3> snap{1.f, 1.f, 1.f};
    std::array<float, 6> bounds{-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    std::array<float, 3> bounds_snap{0.1f, 0.1f, 0.1f};
    bool bound_sizing = false;
    bool bound_sizing_snap = false;

    glm::vec3 *position = nullptr;

    void draw(const glm::mat4 &view, const glm::mat4 proj);
};

class Editor
{
    Window &window;
    Renderer &renderer;

    Gizmo gizmo;

    int bounce_count = 1;
    float distance = 1.f;
    glm::vec3 light_dir{0.f, -1.f, 1.f};

    void draw_profiler();
    void draw_scene_menu();
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