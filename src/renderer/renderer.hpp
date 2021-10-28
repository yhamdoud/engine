#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.hpp"
#include "constants.hpp"
#include "context.hpp"
#include "entity.hpp"
#include "renderer/buffer.hpp"
#include "renderer/passes/bloom.hpp"
#include "renderer/passes/forward.hpp"
#include "renderer/passes/geometry.hpp"
#include "renderer/passes/lighting.hpp"
#include "renderer/passes/motion_blur.hpp"
#include "renderer/passes/shadow.hpp"
#include "renderer/passes/ssao.hpp"
#include "renderer/passes/ssr.hpp"
#include "renderer/passes/tone_map.hpp"
#include "renderer/passes/volumetric.hpp"
#include "renderer/probe_viewport.hpp"

namespace engine
{

class Renderer
{
    enum class Error
    {
        unsupported_texture_format,
    };

    uint depth_tex = invalid_texture_id;

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

    Camera camera;

    ViewportContext ctx_v{
        .near = 0.1f,
        .far = 50.f,
        .fov = glm::radians(90.f),
    };

    RenderContext ctx_r = RenderContext{
        .sun{
            .color = glm::vec3{1.f, 0.95f, 0.95f},
            .intensity = 30.f,
            .direction = glm::normalize(glm::vec3{0.03f, -0.8f, 0.5f}),
        },
        .lights{
            Light{glm::vec3{-5, 0, 0}, glm::vec3{1., 1., 0.}, 2.f},
            Light{glm::vec3{0, 0, 5}, glm::vec3{0., 1., 0.}, 5.f},
            Light{glm::vec3{5, 0, 0}, glm::vec3{0., 1., 1.}, 8.f},
        },
        .sh_texs = std::span<uint, 7>{probe_buf.front(), 7},
        .vertex_buf{32'000 * sizeof(Vertex), 0},
        .index_buf{32'000 * sizeof(uint32_t), 0},
    };

    ShadowPass shadow{{
        .size{4096, 4096},
        .cascade_count = 3,
        .stabilize = true,
        .z_multiplier = 1.3f,
        .cull_front_faces = true,
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

    bool ssr_enabled = true;
    SsrPass ssr{{
        .thickness = 0.25f,
        .stride = 2,
        .do_jitter = true,
        .max_dist = 20.f,
        .max_steps = 300,
        .correct = true,
    }};

    LightingPass lighting{{
        .cascade_count = shadow.params.cascade_count,
        .indirect_light = true,
        .direct_lighting = true,
        .base_color = true,
        .color_cascades = false,
        .filter_shadows = true,
        .leak_offset = 0.5f,
    }};

    ForwardPass forward{{
        .draw_probes = false,
    }};

    VolumetricPass volumetric{{
        .step_count = 64,
        .scatter_intensity = 0.85f,
        .scatter_amount = 0.6f,
        .flags =
            (VolumetricPass::Flags)(VolumetricPass::Flags::bilateral_upsample |
                                    VolumetricPass::Flags::blur),
    }};

    bool motion_blur_enabled = true;
    MotionBlurPass motion_blur{{
        .sample_count = 20,
        .intensity = 0.35f,
    }};

    bool bloom_enabled = true;
    BloomPass bloom{{
        .pass_count = 5u,
        .intensity = 0.04f,
        .upsample_radius = 1.3f,
    }};

    ToneMapPass tone_map{{
        .tm_operator = ToneMapPass::Operator::aces,
        .gamma = 2.2f,
        .min_log_luminance = -10.f,
        .max_log_luminance = 2.f,
        .exposure_adjust_speed = 1.1f,
        .target_luminance = 0.2f,
        .auto_exposure = true,
    }};

    Renderer(glm::ivec2 viewport_size, glm::vec3 camera_position,
             glm::vec3 camera_look);

    // TODO: Build a destruction queue. OS takes cares of this for now...
    ~Renderer() = default;
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    Renderer &operator=(Renderer &&) = delete;

    void render(float dt, std::vector<Entity> queue);

    void update_vao();
    size_t register_mesh(const Mesh &mesh);
    inline static void render_mesh_instance(const MeshInstance &m)
    {
        glDrawElementsBaseVertex(
            GL_TRIANGLES, m.primitive_count, GL_UNSIGNED_INT,
            (void *)(m.index_offset_bytes), m.vertex_offset);
    }

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