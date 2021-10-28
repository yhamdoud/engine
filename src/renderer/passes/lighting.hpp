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
        glm::mat4 proj;
        glm::mat4 proj_inv;
        glm::mat4 view_inv;
        glm::mat4 inv_grid_transform;
        float leak_offset;
        int indirect_lighting;
        int direct_lighting;
        int base_color;
        int color_cascades;
        int filter_shadows;
        int reflections;
        int ambient_occlusion;
        glm::vec3 grid_dims;
        float _pad0;
        glm::vec3 light_intensity;
        float _pad1;
        glm::vec3 light_direction;
        float _pad2;
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
        bool ssao;
        bool ssr;
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