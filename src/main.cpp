#include <filesystem>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "constants.hpp"
#include "editor.hpp"
#include "entity.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "profiler.hpp"
#include "renderer/renderer.hpp"
#include "shader.hpp"
#include "transform.hpp"
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

size_t add_entity(Renderer &renderer, Entity::Flags flags, Transform transform,
                  const Model &model, optional<Entity> parent)
{
    size_t mesh_idx = renderer.register_mesh(*model.mesh);

    size_t sg_idx;

    auto reg = [&renderer](const OptionalTexture *maybe_tex)
    {
        if (const auto &tex = get_if<Texture>(maybe_tex))
            return std::get<uint>(renderer.register_texture(*tex));
        else if (const auto &tex = get_if<CompressedTexture>(maybe_tex))
            return std::get<uint>(renderer.register_texture(*tex));
        else
            return invalid_texture_id;
    };

    uint base_color_tex_id = reg(&model.material.base_color);
    uint normal_tex_id = reg(&model.material.normal);
    uint metallic_roughness_tex_id = reg(&model.material.metallic_roughness);

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
        model.material.base_color_factor,
        model.material.alpha_mode,
        model.material.alpha_cutoff,
    });

    return entities.size() - 1;
}

vector<RenderData> generate_render_data()
{
    ZoneScoped;

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
            e.base_color_factor,
            model,
            e.alpha_mode,
            e.alpha_cutoff,
        });
    }

    return data;
}

int main()
{
    FrameMarkStart("Loading");

    ivec2 size{1600, 900};

    Window window(size, "engine");
    if (!window.load_gl())
    {
        logger.error("OpenGL initialization failed");
        return EXIT_FAILURE;
    }

    Renderer renderer(size);
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

    // {
    //     auto maybe_bistro = load_gltf(models_path /
    //     "bistro/gltf/bistro.gltf");

    //     if (auto bistro = std::get_if<std::vector<Model>>(&maybe_bistro))
    //     {
    //         for (const auto &m : *bistro)
    //         {
    //             add_entity(renderer, Entity::Flags::casts_shadow,
    //                        Transform{m.transform}, m, std::nullopt);
    //         }
    //     }
    // }

    {
        auto maybe_sponza = load_gltf(models_path / "sponza/Sponza.gltf");
        if (auto sponza = std::get_if<std::vector<Model>>(&maybe_sponza))
        {
            for (const auto &m : *sponza)
            {
                add_entity(renderer, Entity::Flags::casts_shadow,
                           Transform{m.transform}, m, std::nullopt);
            }
        }
    }

    // {
    //     auto maybe_helmet = load_gltf(models_path / "damaged_helmet_2.glb");
    //     if (auto helmet = std::get_if<std::vector<Model>>(&maybe_helmet))
    //     {
    //         for (const auto &m : *helmet)
    //         {
    //             Transform t{vec3(1., 0., -2.)};
    //             t.set_euler_angles(radians(90.f), 0.f, 0.f);
    //             add_entity(renderer, Entity::Flags::casts_shadow, t, m,
    //                        std::nullopt);
    //         }
    //     }
    // }

    double last_time = glfwGetTime();
    vec2 cursor_pos = window.get_cursor_position();

    FrameMarkEnd("Loading");

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

            auto data = generate_render_data();
            renderer.render(data);

            editor.draw();
        });

    return EXIT_SUCCESS;
}
