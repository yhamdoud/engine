#include <filesystem>

#include <cxxopts.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "editor.hpp"
#include "entity.hpp"
#include "importer.hpp"
#include "logger.hpp"
#include "renderer/renderer.hpp"
#include "window.hpp"

using std::filesystem::path;

using namespace glm;
using namespace std;
using namespace engine;

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

int main(int argc, char **argv)
{
    cxxopts::Options options("engine", "Deferred rendering engine");
    // clang-format off
    options.add_options()
        ("cam_pos_x", "Camera position x component",
         cxxopts::value<float>()->default_value(("4")))
        ("cam_pos_y", "Camera position y component",
         cxxopts::value<float>()->default_value(("4")))
        ("cam_pos_z", "Camera position z component",
         cxxopts::value<float>()->default_value(("0")))
        ("cam_look_x", "Camera look x component",
         cxxopts::value<float>()->default_value(("0")))
        ("cam_look_y", "Camera look y component",
         cxxopts::value<float>()->default_value(("0")))
        ("cam_look_z", "Camera look z component",
         cxxopts::value<float>()->default_value(("0")))
        ("bake", "Bake irradiance probes");
    // clang-format on

    auto result = options.parse(argc, argv);

    ivec2 size{1600, 900};

    Window window(size, "engine");
    if (!window.load_gl())
    {
        logger.error("OpenGL initialization failed");
        return EXIT_FAILURE;
    }

    Renderer renderer(size,
                      glm::vec3(result["cam_pos_x"].as<float>(),
                                result["cam_pos_y"].as<float>(),
                                result["cam_pos_z"].as<float>()),
                      glm::vec3(result["cam_look_x"].as<float>(),
                                result["cam_look_y"].as<float>(),
                                result["cam_look_z"].as<float>()));

    Editor editor(window, renderer);

    glfwSetWindowUserPointer(window.impl, &renderer);

    const path skybox_path = textures_path / "skybox-1";
    auto skybox_tex =
        upload_cube_map({skybox_path / "right.jpg", skybox_path / "left.jpg",
                         skybox_path / "top.jpg", skybox_path / "bottom.jpg",
                         skybox_path / "front.jpg", skybox_path / "back.jpg"});

    if (skybox_tex == invalid_texture_id)
        return EXIT_FAILURE;

    renderer.ctx_r.skybox_tex = skybox_tex;

    path model = models_path / "sponza/Sponza.gltf";
    // path model = models_path / "bistro/gltf/bistro.gltf";
    // path model = models_path / "avocado.glb";
    // path model = models_path / "boom_box.glb";

    {
        GltfImporter importer(model, renderer);
        if (auto error = importer.import())
            logger.error("Import error");
        else
            copy(importer.models.begin(), importer.models.end(),
                 back_inserter(entities));
    }

    double last_time = glfwGetTime();
    vec2 cursor_pos = window.get_cursor_position();

    renderer.update_vao();

    if (result.count("bake"))
        renderer.prepare_bake(vec3{0.5f, 4.5f, 0.5f}, vec3{22.f, 8.f, 9.f}, 1,
                              1);

    window.run(
        [&]()
        {
            // Timestep.
            float time = glfwGetTime();
            float delta_time = time - last_time;
            last_time = time;

            vec2 new_cursor_pos = window.get_cursor_position();

            if (glfwGetMouseButton(window.impl, GLFW_MOUSE_BUTTON_MIDDLE) ==
                GLFW_PRESS)
            {
                auto delta = cursor_pos - new_cursor_pos;

                if (glfwGetKey(window.impl, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                    renderer.camera.pan(delta);
                else
                    renderer.camera.rotate(delta);
            }

            cursor_pos = new_cursor_pos;

            renderer.render(delta_time, entities);

            editor.draw();
        });

    return EXIT_SUCCESS;
}
