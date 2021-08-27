#define GLFW_INCLUDE_NONE

#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <optional>
#include <string_view>
#include <variant>

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>

#include "constants.hpp"
#include "entity.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "renderer.hpp"
#include "shader.hpp"
#include "transform.hpp"
#include "window.hpp"

using std::array;
using std::optional;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::tuple;
using std::unique_ptr;
using std::vector;
using std::filesystem::path;

using namespace glm;

using namespace engine;

struct SceneGraphNode
{
    static constexpr size_t root_index = (size_t)-1;

    mat4 local_transform_matrix;
    size_t parent_index = root_index;

    void set_position(const vec3 &pos)
    {
        local_transform_matrix[3] = vec4{pos, local_transform_matrix[3][3]};
    }

    SceneGraphNode(mat4 m, size_t i)
        : local_transform_matrix{m}, parent_index{i}
    {
    }
};

// Put in renderer?

vector<Entity> entities;
vector<SceneGraphNode> scene_graph;

size_t scene_graph_insert(const Transform &transform, size_t parent_idx)
{
    scene_graph.emplace_back(transform.get_model(), parent_idx);
    return scene_graph.size() - 1;
}

mat4 get_world_position(size_t node_idx)
{
    auto node = scene_graph[node_idx];
    mat4 world_transform = node.local_transform_matrix;

    while (node.parent_index != SceneGraphNode::root_index)
    {
        node = scene_graph[node.parent_index];
        world_transform = node.local_transform_matrix * world_transform;
    }

    return world_transform;
}

size_t add_entity(Renderer &renderer, Entity::Flags flags, Transform transform,
                  const Mesh &mesh, optional<Entity> parent, Shader &shader)
{
    size_t mesh_idx = renderer.register_mesh(mesh);
    size_t sg_idx;

    if (parent)
        sg_idx = scene_graph_insert(transform, parent->scene_graph_index);
    else
        sg_idx = scene_graph_insert(transform, SceneGraphNode::root_index);

    entities.push_back(Entity{flags, sg_idx, mesh_idx, shader});
    return entities.size() - 1;
}

vector<RenderData> generate_render_data()
{
    vector<RenderData> data;
    data.reserve(entities.size());

    for (const auto &e : entities)
    {
        mat4 model = get_world_position(e.scene_graph_index);

        data.emplace_back(RenderData{
            e.flags,
            e.mesh_index,
            model,
            e.shader,
        });
    }

    return data;
}

int main()
{
    GLFWwindow *window;

    if (auto maybe_window = init_glfw(1280, 720))
        window = *maybe_window;
    else
        return EXIT_FAILURE;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        logger.error("OpenGL initialization failed");
        return EXIT_FAILURE;
    }

    auto skybox_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "skybox.vs",
        .frag = shaders_path / "skybox.fs",
    });

    auto shadow_shader = *Shader::from_paths(
        ShaderPaths{.vert = shaders_path / "shadow_map.vs"});

    const path skybox_path = textures_path / "skybox-1";
    auto skybox_texture =
        upload_cube_map({skybox_path / "right.jpg", skybox_path / "left.jpg",
                         skybox_path / "top.jpg", skybox_path / "bottom.jpg",
                         skybox_path / "front.jpg", skybox_path / "back.jpg"});

    if (skybox_texture == invalid_texture_id)
        return EXIT_FAILURE;

    Renderer renderer{shadow_shader, skybox_shader, skybox_texture};
    glfwSetWindowUserPointer(window, &renderer);

    auto blinn_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "shader.vs",
        .frag = shaders_path / "blinn.fs",
    });

    auto toon_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "shader.vs",
        .frag = shaders_path / "toon.fs",
    });

    auto deferred_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "shader.vs",
        .frag = shaders_path / "deferred.fs",
    });

    auto duck_mesh = Mesh::from_gtlf(models_path / "duck.glb")[0];

    auto duck1 = add_entity(renderer, Entity::Flags::casts_shadow,
                            Transform{vec3{0.f}, vec3{0.01f}}, duck_mesh,
                            std::nullopt, deferred_shader);

    add_entity(renderer, Entity::Flags::none, Transform{vec3{100.f, 5.f, 0.f}},
               duck_mesh, std::make_optional<Entity>(entities[duck1]),
               deferred_shader);

    add_entity(renderer, Entity::Flags::casts_shadow,
               Transform(vec3{0.f}, vec3{10.f}),
               Mesh::from_gtlf(models_path / "plane.gltf")[0], std::nullopt,
               deferred_shader);

    double last_time = glfwGetTime();
    vec2 cursor_pos = get_cursor_position(window);

    // Enable profiling.
    TracyGpuContext;

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    while (!glfwWindowShouldClose(window))
    {
        TracyGpuZone("Render");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Timestep.
        float time = glfwGetTime();
        float delta_time = time - last_time;
        last_time = time;

        vec2 new_cursor_pos = get_cursor_position(window);

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
        {
            auto delta = cursor_pos - new_cursor_pos;

            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                renderer.camera.pan(delta);
            else
                renderer.camera.rotate(delta);
        }

        cursor_pos = new_cursor_pos;

        {
            scene_graph[entities[duck1].scene_graph_index].set_position(
                vec3(0.f, sin(glfwGetTime()), 0.f));
        }

        renderer.viewport_size = vec2{window_data.width, window_data.height};
        auto data = generate_render_data();

        renderer.render(data);

        // Draw UI.
        ImGui::Text("Hello, world %d", 123);
        if (ImGui::Button("Save"))
            ;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();

        TracyGpuCollect;

        FrameMark
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
