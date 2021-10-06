#pragma once

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

struct GeometryConfig
{
};

class GeometryPass
{
    uint f_buf, normal_metal, base_color_rough, velocity, depth;

    Shader shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "geometry.vs",
        .frag = shaders_path / "geometry.fs",
    });

  public:
    GeometryPass();

    uint debug_view_metallic, debug_view_normal, debug_view_base_color,
        debug_view_roughness, debug_view_velocity;

    void create_debug_views();
    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx, RenderContext &r_ctx);
};

static_assert(Pass<GeometryPass>);

} // namespace engine