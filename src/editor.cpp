#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <Tracy.hpp>

#include <ImGuizmo.h>

#include "editor.hpp"
#include "logger.hpp"
#include "profiler.hpp"

using namespace engine;
using namespace std;
using namespace glm;
using namespace fmt;

void Gizmo::draw(const mat4 &view, const mat4 proj)
{
    if (position == nullptr)
        return;

    mat4 matrix(1.f);
    matrix[3] = vec4(*position, 1.f);

    ImGuiIO &io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    ImGuizmo::Manipulate(
        value_ptr(view), value_ptr(proj), ImGuizmo::OPERATION::TRANSLATE,
        ImGuizmo::MODE::LOCAL, value_ptr(matrix), nullptr,
        do_snap ? snap.data() : nullptr, bound_sizing ? bounds.data() : nullptr,
        bound_sizing_snap ? bounds_snap.data() : nullptr);

    *position = vec3(matrix[3]);
}

Editor::~Editor()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

Editor::Editor(Window &w, Renderer &r) : window(w), renderer(r)
{
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(w.impl, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    window.add_mouse_button_callback(
        GLFW_MOUSE_BUTTON_LEFT,
        [&](int, int)
        {
            auto id =
                renderer.id_at_screen_coords(window.get_cursor_position());

            if (id != static_cast<uint32_t>(-1))
                gizmo.position = &renderer.ctx_r.lights[id].position;
        });
}

void Editor::draw()
{
    ZoneScoped;

    TracyGpuZone("Editor");

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    // ImGui::ShowDemoWindow();
    draw_profiler();
    draw_scene_menu();
    draw_renderer_menu();
    gizmo.draw(renderer.ctx_v.view, renderer.ctx_v.proj);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static const char *passes[] = {
    "Baking", "Shadow",      "Geometry", "SSAO",     "Lighting",   "Forward",
    "SSR",    "Motion blur", "Bloom",    "Tone map", "Frame time", "Volumetric",
};

void Editor::draw_profiler()
{

    ImGui::Begin("Profiler");
    {
        for (int i = 0; i < query_count; i++)
        {
            ImGui::Text("%s: %.3f ms", passes[i],
                        static_cast<float>(profiler_zone_time(i)) / 1e6);
        }
    }
    ImGui::End();
}

void Editor::draw_scene_menu()
{
    ImGui::Begin("Scene");

    if (ImGui::TreeNode("Point lights"))
    {
        for (size_t i = 0; i < renderer.ctx_r.lights.size(); i++)
        {
            auto &light = renderer.ctx_r.lights[i];

            string id = format("##Point Light {}", i);

            ImGui::InputFloat3(format("Position{}", id).c_str(),
                               value_ptr(light.position));
            ImGui::ColorEdit3(format("Color{}", id).c_str(),
                              value_ptr(light.color));
            ImGui::SliderFloat(format("Intensity{}", id).c_str(),
                               &light.intensity, 0.f, 100.f);

            ImGui::Separator();
        }

        ImGui::TreePop();
    }

    ImGui::End();
}

void Editor::draw_renderer_menu()
{
    float viewport_aspect_ratio =
        static_cast<float>(renderer.ctx_v.size.x) / renderer.ctx_v.size.y;

    ImGui::Begin("Renderer");

    if (ImGui::CollapsingHeader("Sun"))
    {
        ImGui::ColorEdit3("Color", value_ptr(renderer.ctx_r.sun.color));
        ImGui::SliderFloat("Intensity##Sun", &renderer.ctx_r.sun.intensity, 0.f,
                           100.f);
        if (ImGui::SliderFloat3("Direction", value_ptr(light_dir), -1.f, 1.f))
            renderer.ctx_r.sun.direction = normalize(light_dir);
    }

    if (ImGui::CollapsingHeader("Camera"))
    {
        ImGui::InputFloat3("Position##Camera",
                           value_ptr(renderer.camera.position));
    }

    if (ImGui::CollapsingHeader("GI"))
    {
        ImGui::SliderInt("Bounces", &bounce_count, 1, 10);
        ImGui::SliderFloat("Distance", &distance, 0.5f, 5.f);

        ImGui::Separator();

        ImGui::SliderInt("Batch size", &renderer.bake_batch_size, 1, 512);
        if (ImGui::Button("Bake") && !renderer.is_baking())
        {
            renderer.prepare_bake(vec3{0.5f, 4.5f, 0.5f}, vec3{22.f, 8.f, 9.f},
                                  distance, bounce_count);
        }
        ImGui::Text("Progress");
        ImGui::ProgressBar(renderer.baking_progress());
    }

    if (ImGui::Checkbox("##SSAO", &renderer.ssao_enabled))
        renderer.lighting.params.ssao = renderer.ssao_enabled;
    ImGui::SameLine();
    if (ImGui::CollapsingHeader("SSAO"))
    {
        ImGui::SliderInt("Sample count", &renderer.ssao.data.sample_count, 0,
                         renderer.ssao.data.kernel_size);
        ImGui::SliderFloat("Radius", &renderer.ssao.data.radius, 0.f, 10.f);
        ImGui::SliderFloat("Bias", &renderer.ssao.data.bias, 0.f, 1.f);
        ImGui::SliderFloat("Strength", &renderer.ssao.data.strength, 0.f, 5.f);

        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 texture_size{window_size.x,
                            window_size.x / viewport_aspect_ratio};

        ImGui::Text("Depth");
        ImGui::Image((ImTextureID)renderer.ctx_v.ao_tex, texture_size,
                     ImVec2(0, 1), ImVec2(1, 0));
    }

    if (ImGui::CollapsingHeader("Lighting"))
    {
        auto &params = renderer.lighting.params;
        if (ImGui::Checkbox("Direct lighting", &params.direct_lighting) |
            ImGui::Checkbox("Indirect lighting", &params.indirect_light) |
            ImGui::Checkbox("Base color", &params.base_color) |
            ImGui::Checkbox("Color shadow cascades", &params.color_cascades) |
            ImGui::Checkbox("Filter shadows", &params.filter_shadows) |
            ImGui::SliderFloat("Leak offset ", &params.leak_offset, 0, 1.f))
            renderer.lighting.parse_parameters();
    }

    if (ImGui::CollapsingHeader("Forward"))
    {
        if (ImGui::Checkbox("Draw probes", &renderer.forward.draw_probes))
            renderer.forward.parse_parameters();
    }

    (ImGui::Checkbox("##Volumetric", &renderer.volumetric.enabled));
    ImGui::SameLine();
    if (ImGui::CollapsingHeader("Volumetric"))
    {
        auto &params = renderer.volumetric.params;
        using Flags = VolumetricPass::Flags;

        if (ImGui::SliderFloat("Scatter intensity", &params.scatter_intensity,
                               0.f, 1.f) |
            ImGui::SliderFloat("Scatter amount", &params.scatter_amount, 0.f,
                               5.f) |
            ImGui::SliderInt("Sample count##Volumetric", &params.step_count, 1,
                             64) |
            ImGui::CheckboxFlags("Bilateral upsample", (int *)&params.flags,
                                 Flags::bilateral_upsample) |
            ImGui::CheckboxFlags("Blur", (int *)&params.flags, Flags::blur))
            renderer.volumetric.parse_parameters();
    }

    if (ImGui::Checkbox("##SSR", &renderer.ssr_enabled))
        renderer.lighting.params.ssr = renderer.ssr_enabled;
    ImGui::SameLine();
    if (ImGui::CollapsingHeader("SSR"))
    {
        auto &params = renderer.ssr.cfg;

        if (ImGui::SliderFloat("Thickness", &params.thickness, 0.f, 10.f) |
            ImGui::SliderInt("Stride", &params.stride, 1, 20) |
            ImGui::Checkbox("Jitter", &params.do_jitter) |
            ImGui::SliderFloat("Max distance", &params.max_dist, 0.f, 100.f) |
            ImGui::SliderInt("Max steps", &params.max_steps, 0, 500) |
            ImGui::Checkbox("Fresnel", &params.correct))
            renderer.ssr.parse_parameters();

        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 texture_size{window_size.x,
                            window_size.x / viewport_aspect_ratio};
        ImGui::Image((ImTextureID)renderer.ctx_v.reflections_tex, texture_size,
                     ImVec2(0, 1), ImVec2(1, 0));
    }

    ImGui::Checkbox("##Motion blur", &renderer.motion_blur_enabled);
    ImGui::SameLine();
    if (ImGui::CollapsingHeader("Motion blur"))
    {
        auto &params = renderer.motion_blur.cfg;

        if (ImGui::SliderFloat("Intensity##Motion blur", &params.intensity, 0.f,
                               3.f) |
            ImGui::SliderInt("Sample count##Motion blur", &params.sample_count,
                             1, 25))
            renderer.motion_blur.parse_parameters();
    }

    ImGui::Checkbox("##Bloom", &renderer.bloom_enabled);
    ImGui::SameLine();
    if (ImGui::CollapsingHeader("Bloom"))
    {
        if (ImGui::SliderFloat("Intensity", &renderer.bloom.cfg.intensity, 0.f,
                               0.25f),
            ImGui::SliderFloat("Upsample radius",
                               &renderer.bloom.cfg.upsample_radius, 0.f, 3.f))
            ;
    }

    if (ImGui::CollapsingHeader("Tone map"))
    {
        auto &params = renderer.tone_map.params;

        const char *items[] = {"None", "Reinhard", "ACES", "Uncharted 2"};

        if (ImGui::SliderFloat("Gamma", &params.gamma, 0.01f, 5.f) |
            ImGui::SliderFloat("Exposure adjust speed",
                               &params.exposure_adjust_speed, 0.f, 5.f) |
            ImGui::SliderFloat("Min. log luminance", &params.min_log_luminance,
                               -10.f, 10.f) |
            ImGui::SliderFloat("Max. log luminance", &params.max_log_luminance,
                               -10.f, 10.f) |
            ImGui::SliderFloat("Target luminance", &params.target_luminance,
                               0.01f, 2.f) |
            ImGui::Checkbox("Auto exposure", &params.auto_exposure) |
            ImGui::ListBox("Operator",
                           reinterpret_cast<int *>(&params.tm_operator), items,
                           IM_ARRAYSIZE(items), 4))
            renderer.tone_map.parse_params();
    }

    if (ImGui::CollapsingHeader("Viewport"))
    {
        const array<ivec2, 3> resolutions{ivec2{1280, 720}, ivec2{1600, 900},
                                          ivec2{1920, 1080}};
        const char *items[] = {"1280x720", "1600x900", "1920x1080"};
        static_assert(resolutions.size() == IM_ARRAYSIZE(items));

        static int cur_idx = 0;
        if (ImGui::ListBox("Resolution", &cur_idx, items, IM_ARRAYSIZE(items),
                           4))
        {
            const auto &res = resolutions[cur_idx];
            window.resize(res);
            renderer.resize_viewport(res);
        }

        bool is_full_screen = window.is_full_screen;

        if (ImGui::Checkbox("Full screen", &is_full_screen))
            window.set_full_screen(is_full_screen);
    }

    if (ImGui::CollapsingHeader("Shadow mapping"))
    {
        auto &params = renderer.shadow.params;

        ImGui::Checkbox("Stabilize", &params.stabilize);
        ImGui::SliderFloat("Z-multiplier", &params.z_multiplier, 1.f, 3.f);
        ImGui::Checkbox("Cull front faces", &params.cull_front_faces);

        float aspect_ratio = static_cast<float>(params.size.x) /
                             static_cast<float>(params.size.y);

        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 texture_size{window_size.x, window_size.x / aspect_ratio};

        for (int i = 0; i < params.cascade_count; i++)
        {
            ImGui::Image((ImTextureID)renderer.shadow.debug_views[i],
                         texture_size, ImVec2(0, 1), ImVec2(1, 0));
        }
    }

    if (ImGui::CollapsingHeader("G-buffer"))
    {
        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 texture_size{window_size.x,
                            window_size.x / viewport_aspect_ratio};

        ImGui::Text("Depth");
        ImGui::Image((ImTextureID)renderer.ctx_v.g_buf.depth, texture_size,
                     ImVec2(0, 1), ImVec2(1, 0));

        ImGui::Text("Base color");
        ImGui::Image((ImTextureID)renderer.geometry.debug_view_base_color,
                     texture_size, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::Text("Normal");
        ImGui::Image((ImTextureID)renderer.geometry.debug_view_normal,
                     texture_size, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::Text("Metallic");
        ImGui::Image((ImTextureID)renderer.geometry.debug_view_metallic,
                     texture_size, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::Text("Roughness");
        ImGui::Image((ImTextureID)renderer.geometry.debug_view_roughness,
                     texture_size, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::Text("Velocity");
        ImGui::Image((ImTextureID)renderer.geometry.debug_view_velocity,
                     texture_size, ImVec2(0, 1), ImVec2(1, 0));
    }

    ImGui::End();
}