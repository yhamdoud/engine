#pragma once

#include "constants.hpp"
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
    uint coef_buf = invalid_texture_id;
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
        .near = 0.1f,
        .far = 25.f,
        .fov = glm::radians(90.f),
    };

    ShadowPass shadow{{
        .size = {256, 256},
        .cascade_count = 1,
        .stabilize = true,
        .z_multiplier = 1.5f,
        .cull_front_faces = false,
        .render_point_lights = false,
    }};

    GeometryPass geometry{};

    LightingPass lighting{{
        .cascade_count = shadow.params.cascade_count,
        .indirect_light = false,
        .direct_lighting = true,
        .base_color = true,
        .color_cascades = false,
        .filter_shadows = false,
        .leak_offset = 0.4f,
        .ssao = false,
        .ssr = false,
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