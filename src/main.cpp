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
                  const Model &model, optional<Entity> parent, Shader &shader)
{
    size_t mesh_idx = renderer.register_mesh(*model.mesh);
    auto base_color_tex_id = invalid_texture_id;
    auto normal_tex_id = invalid_texture_id;
    auto metallic_roughness_tex_id = invalid_texture_id;

    size_t sg_idx;

    if (const auto &texture = model.material.base_color)
        base_color_tex_id = std::get<uint>(renderer.register_texture(*texture));

    if (const auto &texture = model.material.normal)
        normal_tex_id = std::get<uint>(renderer.register_texture(*texture));

    if (const auto &texture = model.material.metallic_roughness)
        metallic_roughness_tex_id =
            std::get<uint>(renderer.register_texture(*texture));

    if (parent)
        sg_idx = scene_graph_insert(transform, parent->scene_graph_index);
    else
        sg_idx = scene_graph_insert(transform, SceneGraphNode::root_index);

    entities.emplace_back(Entity{
        flags,
        sg_idx,
        mesh_idx,
        base_color_tex_id,
        normal_tex_id,
        metallic_roughness_tex_id,
        model.material.metallic_factor,
        model.material.roughness_factor,
        shader,
    });

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
            e.base_color_tex_id,
            e.normal_tex_id,
            e.metallic_roughness_tex_id,
            e.metallic_factor,
            e.roughness_factor,
            model,
            e.shader,
        });
    }

    return data;
}

int main()
{
    FrameMarkStart("Loading")

        GLFWwindow *window;

    if (auto maybe_window = init_glfw(1600, 900))
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

    const path skybox_path = textures_path / "skybox-1";
    auto skybox_texture =
        upload_cube_map({skybox_path / "right.jpg", skybox_path / "left.jpg",
                         skybox_path / "top.jpg", skybox_path / "bottom.jpg",
                         skybox_path / "front.jpg", skybox_path / "back.jpg"});

    if (skybox_texture == invalid_texture_id)
        return EXIT_FAILURE;

    Renderer renderer(ivec2{1600, 900}, skybox_shader, skybox_texture);
    glfwSetWindowUserPointer(window, &renderer);

    auto deferred_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "geometry.vs",
        .frag = shaders_path / "geometry.fs",
    });

    auto duck_model = std::move(load_gltf(models_path / "duck.glb")[0]);
    //    auto helmet_model =
    //        std::move(load_gltf(models_path / "damaged_helmet.glb")[0]);

    auto duck1 = add_entity(renderer, Entity::Flags::casts_shadow,
                            Transform{vec3{0.f, 2.f, 0.f}, vec3{0.01f}},
                            duck_model, std::nullopt, deferred_shader);

    auto sponza = load_gltf(models_path / "sponza.glb");
    for (const auto &m : sponza)
    {
        add_entity(renderer, Entity::Flags::casts_shadow,
                   Transform{m.transform}, m, std::nullopt, deferred_shader);
    }

    //    auto helmet = add_entity(renderer, Entity::Flags::casts_shadow,
    //                             Transform{vec3{2.f, 1.f, 0.f}}, helmet_model,
    //                             std::nullopt, deferred_shader);

    //    add_entity(renderer, Entity::Flags::none, Transform{vec3{100.f, 5.f,
    //    0.f}},
    //               duck_model, std::make_optional<Entity>(entities[duck1]),
    //               deferred_shader);
    //
    //    add_entity(renderer, Entity::Flags::casts_shadow,
    //               Transform(vec3{0.f}, vec3{10.f}),
    //               load_gltf(models_path / "plane.gltf")[0], std::nullopt,
    //               deferred_shader);

    auto sphere_model = std::move(load_gltf(models_path / "sphere.glb")[0]);
    add_entity(renderer, Entity::Flags::none, Transform(renderer.sun.position),
               sphere_model, std::nullopt, deferred_shader);

    double last_time = glfwGetTime();
    vec2 cursor_pos = get_cursor_position(window);

    // Enable profiling.
    TracyGpuContext;

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    FrameMarkEnd("Loading")

        while (!glfwWindowShouldClose(window))
    {
        TracyGpuZone("Render");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Renderer");
        {
            if (ImGui::CollapsingHeader("Viewport"))
            {
                const array<ivec2, 3> resolutions{
                    ivec2{1280, 720}, ivec2{1600, 900}, ivec2{1920, 1080}};
                const char *items[] = {"1280x720", "1600x900", "1920x1080"};
                static_assert(resolutions.size() == IM_ARRAYSIZE(items));

                static int cur_idx = 0;
                ImGui::ListBox("Resolution", &cur_idx, items,
                               IM_ARRAYSIZE(items), 4);
                if (ImGui::Button("Set"))
                {
                    const auto &res = resolutions[cur_idx];
                    glfwSetWindowSize(window, res.x, res.y);
                    renderer.resize_viewport(res);
                }
            }

            if (ImGui::CollapsingHeader("Lighting"))
            {
                ImGui::Image((ImTextureID)renderer.shadow_map,
                             ImVec2{(float)renderer.shadow_map_size.x,
                                    (float)renderer.shadow_map_size.y},
                             ImVec2(0, 1), ImVec2(1, 0));
            }

            if (ImGui::CollapsingHeader("G-buffer"))
            {
                float aspect_ratio =
                    static_cast<float>(renderer.viewport_size.x) /
                    renderer.viewport_size.y;

                ImGui::BeginChild("Stuff");
                ImVec2 window_size = ImGui::GetWindowSize();
                ImVec2 texture_size{window_size.x,
                                    window_size.x / aspect_ratio};

                ImGui::Text("Depth");
                ImGui::Image((ImTextureID)renderer.g_depth, texture_size,
                             ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Base color");
                ImGui::Image((ImTextureID)renderer.debug_view_base_color,
                             texture_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Normal");
                ImGui::Image((ImTextureID)renderer.debug_view_normal,
                             texture_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Metallic");
                ImGui::Image((ImTextureID)renderer.debug_view_metallic,
                             texture_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Roughness");
                ImGui::Image((ImTextureID)renderer.debug_view_roughness,
                             texture_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::EndChild();
            }
        }
        ImGui::End();

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

        auto data = generate_render_data();

        renderer.render(data);

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
