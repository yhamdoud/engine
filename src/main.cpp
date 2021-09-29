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

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>

#include "constants.hpp"
#include "editor.hpp"
#include "entity.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "renderer/renderer.hpp"
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

void gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                         GLsizei length, GLchar const *message,
                         void const *user_param)
{
    const string source_string{[&source]
                               {
                                   switch (source)
                                   {
                                   case GL_DEBUG_SOURCE_API:
                                       return "API";
                                   case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
                                       return "Window system";
                                   case GL_DEBUG_SOURCE_SHADER_COMPILER:
                                       return "Shader compiler";
                                   case GL_DEBUG_SOURCE_THIRD_PARTY:
                                       return "Third party";
                                   case GL_DEBUG_SOURCE_APPLICATION:
                                       return "Application";
                                   case GL_DEBUG_SOURCE_OTHER:
                                       return "";
                                   }
                               }()};

    const string type_string{[&type]
                             {
                                 switch (type)
                                 {
                                 case GL_DEBUG_TYPE_ERROR:
                                     return "Error";
                                 case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
                                     return "Deprecated behavior";
                                 case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                                     return "Undefined behavior";
                                 case GL_DEBUG_TYPE_PORTABILITY:
                                     return "Portability";
                                 case GL_DEBUG_TYPE_PERFORMANCE:
                                     return "Performance";
                                 case GL_DEBUG_TYPE_MARKER:
                                     return "Marker";
                                 case GL_DEBUG_TYPE_OTHER:
                                     return "";
                                 }
                             }()};

    const auto log_type = [&severity]
    {
        switch (severity)
        {
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return LogType::info;
        case GL_DEBUG_SEVERITY_LOW:
            return LogType::warning;
        case GL_DEBUG_SEVERITY_MEDIUM:
        case GL_DEBUG_SEVERITY_HIGH:
            return LogType::error;
        }
    }();

    logger.log(log_type, "OpenGL {0} {1}: {3}", type_string, source_string, id,
               message);
}

// TODO: move

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
        shader,
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
            e.shader,
        });
    }

    return data;
}

int main()
{
    FrameMarkStart("Loading");

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

    // Enable error callback.
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_message_callback, nullptr);

    const path skybox_path = textures_path / "skybox-1";
    auto skybox_texture =
        upload_cube_map({skybox_path / "right.jpg", skybox_path / "left.jpg",
                         skybox_path / "top.jpg", skybox_path / "bottom.jpg",
                         skybox_path / "front.jpg", skybox_path / "back.jpg"});

    if (skybox_texture == invalid_texture_id)
        return EXIT_FAILURE;

    Renderer renderer(ivec2{1600, 900}, skybox_texture);
    glfwSetWindowUserPointer(window, &renderer);

    Editor editor(*window, renderer);

    auto deferred_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "geometry.vs",
        .frag = shaders_path / "geometry.fs",
    });

    // {
    //     auto maybe_bistro = load_gltf(models_path /
    //     "bistro/gltf/bistro.gltf");

    //     if (auto bistro = std::get_if<std::vector<Model>>(&maybe_bistro))
    //     {
    //         for (const auto &m : *bistro)
    //         {
    //             add_entity(r, Entity::Flags::casts_shadow,
    //                        Transform{m.transform}, m, std::nullopt,
    //                        deferred_shader);
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
                           Transform{m.transform}, m, std::nullopt,
                           deferred_shader);
            }
        }
    }

    double last_time = glfwGetTime();
    vec2 cursor_pos = get_cursor_position(window);

    // Enable profiling.
    TracyGpuContext;

    FrameMarkEnd("Loading");

    while (!glfwWindowShouldClose(window))
    {
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

        auto data = generate_render_data();

        renderer.render(data);
        editor.draw();

        glfwSwapBuffers(window);
        glfwPollEvents();

        TracyGpuCollect;

        FrameMark
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
