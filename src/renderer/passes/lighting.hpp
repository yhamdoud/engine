#pragma once

#include <fmt/format.h>

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

class LightingPass
{
    struct Uniforms
    {
        glm::mat4 proj{};
        glm::mat4 proj_inv{};
        glm::mat4 view_inv{};
        glm::mat4 inv_grid_transform{};
        float leak_offset = 0.f;
        int indirect_lighting = false;
        int direct_lighting = false;
        int base_color = false;
        int color_cascades = false;
        int filter_shadows = false;
        int reflections = false;
        int ambient_occlusion = false;
        glm::vec3 grid_dims{};
        int _pad0 = 0.f;
        glm::vec3 light_intensity{};
        int _pad1 = 0.f;
        glm::vec3 light_direction{};
        int _pad2 = 0.f;
    };

    struct Params
    {
        int cascade_count;
        bool indirect_light;
        bool direct_lighting;
        bool base_color;
        bool color_cascades;
        bool filter_shadows;
        float leak_offset;
        // FIXME:
        bool ssao = true;
        bool ssr = true;
    };

    Shader lighting_shader;

    Shader point_light_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "point_light.vs",
        .frag = shaders_path / "point_light.fs",
    });

    Uniforms uniforms;
    uint uniform_buf;

  public:
    Params params;

    LightingPass(Params cfg);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx_v, RenderContext &ctx_r);
};

static_assert(Pass<LightingPass>);

} // namespace engine