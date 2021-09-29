#pragma once

#include <variant>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.hpp"
#include "context.hpp"
#include "passes/bloom.hpp"
#include "passes/forward.hpp"
#include "passes/geometry.hpp"
#include "passes/lighting.hpp"
#include "passes/shadow.hpp"
#include "passes/ssao.hpp"
#include "passes/tone_map.hpp"

namespace engine
{

struct ProbeGrid
{
    uint coef_buf;
    int bounce_count;
    glm::ivec3 dims;
    glm::ivec3 group_count;
    glm::mat4 grid_transform;
    float weight_sum;
};

struct ProbeViewport
{
    ViewportContext ctx{
        .size = glm::ivec2(64, 64),
        .near = 0.01f,
        .far = 50.f,
        .fov = glm::radians(90.f),
    };

    ShadowPass shadow{{
        .size = {256, 256},
        .stabilize = true,
    }};

    GeometryPass geometry{};

    LightingPass lighting{{
        .indirect_light = false,
        .direct_light = true,
        .use_base_color = true,
        .color_shadow_cascades = false,
        .filter_shadows = false,
        .leak_offset = 0.4f,
    }};

    ForwardPass forward{{
        .draw_probes = false,
    }};
};

class Renderer
{
    enum class Error
    {
        unsupported_texture_format,
    };

    Shader project = *Shader::from_comp_path(shaders_path / "sh_project.comp");
    Shader reduce = *Shader::from_comp_path(shaders_path / "sh_reduce.comp");

    ProbeViewport probe_view;
    ProbeGrid grid_data;

    bool bake_probes_flag = false;

    void bake_probes();

  public:
    Camera camera{glm::vec3{0, 0, 4}, glm::vec3{0}};

    ViewportContext ctx_v{
        .near = 0.01f,
        .far = 50.f,
        .fov = glm::radians(90.f),
    };

    RenderContext ctx_r = RenderContext{
        .sun =
            {
                .position = glm::vec3(0.f, 15., -5.f),
                .color = glm::vec3{1.f, 0.95f, 0.95f},
                .intensity = 40.f,
                .direction = glm::normalize(glm::vec3{0.2f, -1.f, 0.2f}),
            },
        .lights =
            std::vector<Light>{
                Light{glm::vec3{0, 3, 3}, glm::vec3{0., 0., 0.}},
                Light{glm::vec3{0, 3, 3}, glm::vec3{0., 0., 0.}},
                Light{glm::vec3{3, 3, 3}, glm::vec3{0., 0., 0.}},
            },
    };

    ShadowPass shadow{{
        .size = {4096, 4096},
        .stabilize = true,
    }};

    GeometryPass geometry{};

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

    Renderer(glm::ivec2 viewport_size, uint skybox_texture);
    ~Renderer();

    Renderer(Renderer &&) = delete;
    Renderer &operator=(const Renderer &) = delete;

    void render(std::vector<RenderData> &queue);

    Renderer &operator=(Renderer &&) = delete;

    size_t register_mesh(const Mesh &mesh);
    static void render_mesh_instance(unsigned int vao,
                                     const MeshInstance &mesh);

    IrradianceProbe generate_probe(glm::vec3 position);
    void initialize_probes(glm::vec3 center, glm::vec3 world_dims,
                           float distance, int bounce_count);

    void resize_viewport(glm::vec2 size);

    std::variant<uint, Error> register_texture(const Texture &texture);
    std::variant<uint, Error>
    register_texture(const CompressedTexture &texture);
};

} // namespace engine