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
        model.material.base_color_factor,
        model.material.alpha_mode,
        model.material.alpha_cutoff,
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

    Renderer r(ivec2{1600, 900}, skybox_texture);
    glfwSetWindowUserPointer(window, &r);

    auto deferred_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "geometry.vs",
        .frag = shaders_path / "geometry.fs",
    });

    {

        //    auto helmet_model =
        //        std::move(load_gltf(models_path / "damaged_helmet.glb")[0]);

        auto duck_model = std::move(load_gltf(models_path / "duck.glb")[0]);

        auto sphere_model = std::move(load_gltf(models_path / "sphere.glb")[0]);

        auto sponza = load_gltf(models_path / "sponza.glb");
        for (const auto &m : sponza)
        {
            add_entity(r, Entity::Flags::casts_shadow, Transform{m.transform},
                       m, std::nullopt, deferred_shader);
        }

        //        auto duck1 = add_entity(r, Entity::Flags::casts_shadow,
        //                                Transform{vec3{0.f, 2.f, 0.f},
        //                                vec3{0.01f}}, duck_model,
        //                                std::nullopt, deferred_shader);

        //        add_entity(r, Entity::Flags::none, Transform(vec3{0, 4, 2}),
        //                   sphere_model, std::nullopt, deferred_shader);
        //
        //        add_entity(r, Entity::Flags::none, Transform(vec3{0, 4, -4},
        //        vec3{0.5}),
        //                   sphere_model, std::nullopt, deferred_shader);

        //    auto helmet = add_entity(renderer, Entity::Flags::casts_shadow,
        //                             Transform{vec3{2.f, 1.f, 0.f}},
        //                             helmet_model, std::nullopt,
        //                             deferred_shader);

        //    add_entity(renderer, Entity::Flags::none,
        //    Transform{vec3{100.f, 5.f, 0.f}},
        //               duck_model,
        //               std::make_optional<Entity>(entities[duck1]),
        //               deferred_shader);
        //
        //    add_entity(renderer, Entity::Flags::casts_shadow,
        //               Transform(vec3{0.f}, vec3{10.f}),
        //               load_gltf(models_path / "plane.gltf")[0], std::nullopt,
        //               deferred_shader);

        // auto gi_test = load_gltf(models_path / "gi_test.glb");
        // for (const auto &m : gi_test)
        //     add_entity(r, Entity::Flags::casts_shadow, Transform(vec3{0.f}),
        //     m,
        //                std::nullopt, deferred_shader);

        // add_entity(r, Entity::Flags::none, Transform(r.sun.position),
        //            sphere_model, std::nullopt, deferred_shader);
    }

    double last_time = glfwGetTime();
    vec2 cursor_pos = get_cursor_position(window);

    // Enable profiling.
    TracyGpuContext;

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    FrameMarkEnd("Loading");

    int bounce_count = 1;
    float distance = 1.f;
    //    r.generate_probe_grid(data, vec3{2.f, 6.f, 2.f}, vec3{4, 4, 4}, 4);

    // r.generate_probe_grid_gpu(data, vec3{0.5f, 4.5f, 0.5f},
    //                           vec3{22.f, 8.f, 9.f}, 1.f);

    // r.generate_probe_grid_gpu(data, vec3{0.5f, 4.5f, 0.5f},
    //                           vec3{22.f, 8.f, 9.f}, 3.f);

    while (!glfwWindowShouldClose(window))
    {
        TracyGpuZone("Render");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        float viuewport_aspect_ratio =
            static_cast<float>(r.ctx_v.size.x) / r.ctx_v.size.y;

        ImGui::Begin("Renderer");
        {
            if (ImGui::CollapsingHeader("GI"))
            {
                ImGui::SliderInt("Bounces", &bounce_count, 0, 10);
                ImGui::SliderFloat("Distance", &distance, 1.f, 5.f);

                if (ImGui::Button("Bake"))
                {
                    auto data = generate_render_data();
                    r.generate_probe_grid_gpu(data, vec3{0.5f, 4.5f, 0.5f},
                                              vec3{22.f, 8.f, 9.f}, distance,
                                              bounce_count);
                }
            }

            if (ImGui::CollapsingHeader("SSAO"))
            {
                if (ImGui::SliderInt("Sample count", &r.ssao.sample_count, 0,
                                     r.ssao.sample_count) |
                    ImGui::SliderFloat("Radius", &r.ssao.radius, 0.f, 10.f) |
                    ImGui::SliderFloat("Bias", &r.ssao.bias, 0.f, 1.f) |
                    ImGui::SliderFloat("Strength", &r.ssao.strength, 0.f, 5.f))
                    r.ssao.parse_parameters();

                ImVec2 window_size = ImGui::GetWindowSize();
                ImVec2 texture_size{window_size.x,
                                    window_size.x / viuewport_aspect_ratio};

                ImGui::Text("Depth");
                ImGui::Image((ImTextureID)r.ctx_v.ao_tex, texture_size,
                             ImVec2(0, 1), ImVec2(1, 0));
            }

            if (ImGui::CollapsingHeader("Lighting"))
            {
                if (ImGui::Checkbox("Direct lighting",
                                    &r.lighting.direct_light);
                    ImGui::Checkbox("Indirect lighting",
                                    &r.lighting.indirect_light) |
                    ImGui::Checkbox("Base color", &r.lighting.use_base_color) |
                    ImGui::Checkbox("Color shadow cascades",
                                    &r.lighting.color_shadow_cascades) |
                    ImGui::Checkbox("Filter shadows",
                                    &r.lighting.filter_shadows) |
                    ImGui::SliderFloat("Leak offset ", &r.lighting.leak_offset,
                                       0, 1.f))
                    r.lighting.parse_parameters();
            }

            if (ImGui::CollapsingHeader("Forward"))
            {
                if (ImGui::Checkbox("Draw probes", &r.forward.draw_probes))
                    r.forward.parse_parameters();
            }

            if (ImGui::CollapsingHeader("Bloom"))
            {
                if (ImGui::SliderFloat("Intensity", &r.bloom.cfg.intensity, 0.f,
                                       5.f),
                    ImGui::SliderFloat("Upsample radius",
                                       &r.bloom.cfg.upsample_radius, 0.f, 3.f))
                    ;
            }

            if (ImGui::CollapsingHeader("Tone map"))
            {
                if (ImGui::Checkbox("Gamma correction",
                                    &r.tone_map.do_gamma_correct) |
                    ImGui::Checkbox("Tone mapping", &r.tone_map.do_tone_map) |
                    ImGui::InputFloat("Gamma", &r.tone_map.gamma) |
                    ImGui::InputFloat("Exposure", &r.tone_map.exposure))
                    r.tone_map.parse_parameters();
            }

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
                    r.resize_viewport(res);
                }
            }

            if (ImGui::CollapsingHeader("Shadow mapping"))
            {
                ImGui::Checkbox("Stabilize", &r.shadow.stabilize);

                float aspect_ratio =
                    static_cast<float>(r.shadow.size.x) / r.shadow.size.y;

                ImVec2 window_size = ImGui::GetWindowSize();
                ImVec2 texture_size{window_size.x,
                                    window_size.x / aspect_ratio};

                for (const auto &v : r.shadow.debug_views)
                {
                    ImGui::Image((ImTextureID)v, texture_size, ImVec2(0, 1),
                                 ImVec2(1, 0));
                }
            }

            if (ImGui::CollapsingHeader("G-buffer"))
            {
                ImGui::BeginChild("Stuff");
                ImVec2 window_size = ImGui::GetWindowSize();
                ImVec2 texture_size{window_size.x,
                                    window_size.x / viuewport_aspect_ratio};

                ImGui::Text("Depth");
                ImGui::Image((ImTextureID)r.ctx_v.g_buf.depth, texture_size,
                             ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Base color");
                ImGui::Image((ImTextureID)r.geometry.debug_view_base_color,
                             texture_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Normal");
                ImGui::Image((ImTextureID)r.geometry.debug_view_normal,
                             texture_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Metallic");
                ImGui::Image((ImTextureID)r.geometry.debug_view_metallic,
                             texture_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Roughness");
                ImGui::Image((ImTextureID)r.geometry.debug_view_roughness,
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
                r.camera.pan(delta);
            else
                r.camera.rotate(delta);
        }

        cursor_pos = new_cursor_pos;

        //        {
        //            scene_graph[entities[duck1].scene_graph_index].set_position(
        //                vec3(0.f, sin(glfwGetTime()), 0.f));
        //        }

        auto data = generate_render_data();

        r.render(data);

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
