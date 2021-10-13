#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>

#include "editor.hpp"
#include "glm/geometric.hpp"
#include "profiler.hpp"

using namespace engine;
using namespace std;
using namespace glm;

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
}

void Editor::draw()
{
    ZoneScoped;

    TracyGpuZone("Editor");

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    draw_profiler();
    draw_renderer_menu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static const char *passes[] = {
    "Baking", "Shadow",      "Geometry", "SSAO",     "Lighting",  "Forward",
    "SSR",    "Motion blur", "Bloom",    "Tone map", "Frame time"};

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

void Editor::draw_renderer_menu()
{
    float viewport_aspect_ratio =
        static_cast<float>(renderer.ctx_v.size.x) / renderer.ctx_v.size.y;

    ImGui::Begin("Renderer");
    {
        if (ImGui::CollapsingHeader("Sun"))
        {
            ImGui::ColorEdit3("Color", value_ptr(renderer.ctx_r.sun.color));
            ImGui::SliderFloat("Intensity##Sun", &renderer.ctx_r.sun.intensity,
                               0.f, 100.f);
            if (ImGui::SliderFloat3("Direction", value_ptr(light_dir), -1.f,
                                    1.f))
                renderer.ctx_r.sun.direction = normalize(light_dir);
        }

        if (ImGui::CollapsingHeader("GI"))
        {
            ImGui::SliderInt("Bounces", &bounce_count, 1, 10);
            ImGui::SliderFloat("Distance", &distance, 0.5f, 5.f);

            ImGui::Separator();

            ImGui::SliderInt("Batch size", &renderer.bake_batch_size, 1, 512);
            if (ImGui::Button("Bake") && !renderer.is_baking())
            {
                renderer.prepare_bake(vec3{0.5f, 4.5f, 0.5f},
                                      vec3{22.f, 8.f, 9.f}, distance,
                                      bounce_count);
            }
            ImGui::Text("Progress");
            ImGui::ProgressBar(renderer.baking_progress());
        }

        if (ImGui::Checkbox("##SSAO", &renderer.ssao_enabled))
            renderer.lighting.ssao = renderer.ssao_enabled;
        ImGui::SameLine();
        if (ImGui::CollapsingHeader("SSAO"))
        {
            ImGui::SliderInt("Sample count", &renderer.ssao.data.sample_count,
                             0, renderer.ssao.data.kernel_size);
            ImGui::SliderFloat("Radius", &renderer.ssao.data.radius, 0.f, 10.f);
            ImGui::SliderFloat("Bias", &renderer.ssao.data.bias, 0.f, 1.f);
            ImGui::SliderFloat("Strength", &renderer.ssao.data.strength, 0.f,
                               5.f);

            ImVec2 window_size = ImGui::GetWindowSize();
            ImVec2 texture_size{window_size.x,
                                window_size.x / viewport_aspect_ratio};

            ImGui::Text("Depth");
            ImGui::Image((ImTextureID)renderer.ctx_v.ao_tex, texture_size,
                         ImVec2(0, 1), ImVec2(1, 0));
        }

        if (ImGui::CollapsingHeader("Lighting"))
        {
            if (ImGui::Checkbox("Direct lighting",
                                &renderer.lighting.direct_light) |
                ImGui::Checkbox("Indirect lighting",
                                &renderer.lighting.indirect_light) |
                ImGui::Checkbox("Base color",
                                &renderer.lighting.use_base_color) |
                ImGui::Checkbox("Color shadow cascades",
                                &renderer.lighting.color_shadow_cascades) |
                ImGui::Checkbox("Filter shadows",
                                &renderer.lighting.filter_shadows) |
                ImGui::SliderFloat("Leak offset ",
                                   &renderer.lighting.leak_offset, 0, 1.f))
                renderer.lighting.parse_parameters();
        }

        if (ImGui::CollapsingHeader("Forward"))
        {
            if (ImGui::Checkbox("Draw probes", &renderer.forward.draw_probes))
                renderer.forward.parse_parameters();
        }

        if (ImGui::Checkbox("##SSR", &renderer.ssr_enabled))
            renderer.lighting.ssr = renderer.ssr_enabled;
        ImGui::SameLine();
        if (ImGui::CollapsingHeader("SSR"))
        {
            if (ImGui::SliderFloat("Thickness", &renderer.ssr.cfg.thickness,
                                   0.f, 10.f) |
                ImGui::SliderInt("Stride", &renderer.ssr.cfg.stride, 1, 20) |
                ImGui::Checkbox("Jitter", &renderer.ssr.cfg.do_jitter) |
                ImGui::SliderFloat("Max distance", &renderer.ssr.cfg.max_dist,
                                   0.f, 100.f) |
                ImGui::SliderInt("Max steps", &renderer.ssr.cfg.max_steps, 0,
                                 500) |
                ImGui::Checkbox("Fresnel", &renderer.ssr.cfg.correct))
                renderer.ssr.parse_parameters();

            ImVec2 window_size = ImGui::GetWindowSize();
            ImVec2 texture_size{window_size.x,
                                window_size.x / viewport_aspect_ratio};
            ImGui::Image((ImTextureID)renderer.ctx_v.reflections_tex,
                         texture_size, ImVec2(0, 1), ImVec2(1, 0));
        }

        ImGui::Checkbox("##Motion blur", &renderer.motion_blur_enabled);
        ImGui::SameLine();
        if (ImGui::CollapsingHeader("Motion blur"))
        {
            if (ImGui::SliderFloat("Intensity##Motion blur",
                                   &renderer.motion_blur.cfg.intensity, 0.f,
                                   3.f) |
                ImGui::SliderInt("Sample count##Motion blur",
                                 &renderer.motion_blur.cfg.sample_count, 1, 25))
                renderer.motion_blur.parse_parameters();
        }

        ImGui::Checkbox("##Bloom", &renderer.bloom_enabled);
        ImGui::SameLine();
        if (ImGui::CollapsingHeader("Bloom"))
        {
            if (ImGui::SliderFloat("Intensity", &renderer.bloom.cfg.intensity,
                                   0.f, 0.25f),
                ImGui::SliderFloat("Upsample radius",
                                   &renderer.bloom.cfg.upsample_radius, 0.f,
                                   3.f))
                ;
        }

        if (ImGui::CollapsingHeader("Tone map"))
        {
            if (ImGui::Checkbox("Gamma correction",
                                &renderer.tone_map.params.do_gamma_correct) |
                ImGui::Checkbox("Tone mapping",
                                &renderer.tone_map.params.do_tone_map) |
                ImGui::InputFloat("Gamma", &renderer.tone_map.params.gamma) |
                ImGui::SliderFloat(
                    "Exposure adjust speed",
                    &renderer.tone_map.params.exposure_adjust_speed, 0.f, 5.f) |
                ImGui::SliderFloat("Min. log luminance",
                                   &renderer.tone_map.params.min_log_luminance,
                                   -10.f, 10.f) |
                ImGui::SliderFloat("Max. log luminance",
                                   &renderer.tone_map.params.max_log_luminance,
                                   -10.f, 10.f) |
                ImGui::SliderFloat("Target luminance",
                                   &renderer.tone_map.params.target_luminance,
                                   0.01f, 2.f))
                renderer.tone_map.parse_params();
        }

        if (ImGui::CollapsingHeader("Viewport"))
        {
            const array<ivec2, 3> resolutions{
                ivec2{1280, 720}, ivec2{1600, 900}, ivec2{1920, 1080}};
            const char *items[] = {"1280x720", "1600x900", "1920x1080"};
            static_assert(resolutions.size() == IM_ARRAYSIZE(items));

            static int cur_idx = 0;
            ImGui::ListBox("Resolution", &cur_idx, items, IM_ARRAYSIZE(items),
                           4);
            if (ImGui::Button("Set"))
            {
                const auto &res = resolutions[cur_idx];
                // TODO: resize window
                renderer.resize_viewport(res);
            }
        }

        if (ImGui::CollapsingHeader("Shadow mapping"))
        {
            ImGui::Checkbox("Stabilize", &renderer.shadow.stabilize);

            float aspect_ratio = static_cast<float>(renderer.shadow.size.x) /
                                 renderer.shadow.size.y;

            ImVec2 window_size = ImGui::GetWindowSize();
            ImVec2 texture_size{window_size.x, window_size.x / aspect_ratio};

            for (const auto &v : renderer.shadow.debug_views)
            {
                ImGui::Image((ImTextureID)v, texture_size, ImVec2(0, 1),
                             ImVec2(1, 0));
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
    }
    ImGui::End();
}
