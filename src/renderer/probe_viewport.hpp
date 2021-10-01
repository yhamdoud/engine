#pragma once

#include "renderer/context.hpp"
#include "renderer/passes/forward.hpp"
#include "renderer/passes/geometry.hpp"
#include "renderer/passes/lighting.hpp"
#include "renderer/passes/shadow.hpp"
#include "renderer/probe_buffer.hpp"
#include "shader.hpp"

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
    Shader project;
    Shader reduce;

    glm::vec3 position;

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

    ProbeViewport();

    void render(RenderContext &ctx_r);
    void bake(RenderContext &ctx_r, const BakingJob &job, int bounce_idx,
              const ProbeGrid &grid, ProbeBuffer &buf);
};

} // namespace engine