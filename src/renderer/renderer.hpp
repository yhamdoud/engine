#pragma once

#include <variant>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.hpp"
#include "context.hpp"
#include "renderer/passes/bloom.hpp"
#include "renderer/passes/forward.hpp"
#include "renderer/passes/geometry.hpp"
#include "renderer/passes/lighting.hpp"
#include "renderer/passes/motion_blur.hpp"
#include "renderer/passes/shadow.hpp"
#include "renderer/passes/ssao.hpp"
#include "renderer/passes/ssr.hpp"
#include "renderer/passes/tone_map.hpp"
#include "renderer/probe_viewport.hpp"

namespace engine
{

class Renderer
{
    enum class Error
    {
        unsupported_texture_format,
    };

    ProbeViewport probe_view;
    ProbeGrid probe_grid;
    ProbeBuffer probe_buf{glm::ivec3(1, 1, 1)};
    std::vector<BakingJob> baking_jobs{};
    int cur_baking_offset = 0;
    int cur_bounce_idx = 0;

    void bake();

  public:
    int bake_batch_size = 32;
    int probe_view_count = 10;

    Camera camera{glm::vec3{0, 0, 4}, glm::vec3{0}};

    ViewportContext ctx_v{
        .near = 0.1f,
        .far = 50.f,
        .fov = glm::radians(90.f),
    };

    RenderContext ctx_r = RenderContext{
        .sun{
            .color = glm::vec3{1.f, 0.95f, 0.95f},
            .intensity = 40.f,
            .direction = glm::normalize(glm::vec3{0.2f, -1.f, 0.2f}),
        },
        .lights{
            Light{glm::vec3{0, 3, 3}, glm::vec3{0., 0., 0.}},
            Light{glm::vec3{0, 3, 3}, glm::vec3{0., 0., 0.}},
            Light{glm::vec3{3, 3, 3}, glm::vec3{0., 0., 0.}},
        },
        .sh_texs = std::span<uint, 7>{probe_buf.front(), 7},
    };

    ShadowPass shadow{{
        .size{4096, 4096},
        .stabilize = true,
    }};

    GeometryPass geometry{};

    bool ssao_enabled = true;
    SsaoPass ssao{{
        .kernel_size = 64,
        .sample_count = 64,
        .radius = 0.5f,
        .bias = 0.01f,
        .strength = 2.f,
    }};

    LightingPass lighting{{
        .indirect_light = true,
        .direct_light = true,
        .use_base_color = true,
        .color_shadow_cascades = false,
        .filter_shadows = true,
        .leak_offset = 0.4f,
    }};

    ForwardPass forward{{
        .draw_probes = false,
    }};

    bool ssr_enabled = true;
    SsrPass ssr{{
        .thickness = 0.25f,
        .stride = 2,
        .do_jitter = true,
        .max_dist = 20.f,
        .max_steps = 300,
        .correct = true,
    }};

    bool motion_blur_enabled = true;
    MotionBlurPass motion_blur{{
        .sample_count = 20,
        .intensity = 0.35f,
    }};

    bool bloom_enabled = true;
    BloomPass bloom{{
        .pass_count = 5u,
        .intensity = 0.05f,
        .upsample_radius = 1.5f,
    }};

    ToneMapPass tone_map{{
        .do_tonemap = true,
        .do_gamma_correct = true,
        .exposure = 1.f,
        .gamma = 2.2f,
    }};

    Renderer(glm::ivec2 viewport_size);
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    Renderer &operator=(Renderer &&) = delete;

    void render(std::vector<RenderData> &queue);

    size_t register_mesh(const Mesh &mesh);
    static void render_mesh_instance(unsigned int vao,
                                     const MeshInstance &mesh);

    void prepare_bake(glm::vec3 center, glm::vec3 world_dims, float distance,
                      int bounce_count);

    void resize_viewport(glm::vec2 size);

    std::variant<uint, Error> register_texture(const Texture &texture);
    std::variant<uint, Error>
    register_texture(const CompressedTexture &texture);

    float baking_progress();
    bool is_baking();
};

} // namespace engine