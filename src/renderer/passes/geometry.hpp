#pragma once

#include <fmt/format.h>

#include "constants.hpp"
#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

struct GeometryConfig
{
};

class GeometryPass
{
    static constexpr int group_size = 32;

    uint fbuf;
    uint fbuf_downsample;

    uint normal_metal = invalid_texture_id;
    uint base_color_rough = invalid_texture_id;
    uint velocity = invalid_texture_id;
    uint depth = invalid_texture_id;

    Shader shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "geometry.vs",
        .frag = shaders_path / "geometry.fs",
    });

    // FIXME: duplicate
    Shader point_light_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "point_light.vs",
        .frag = shaders_path / "point_light_id.frag",
    });

    Shader downsample_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "lighting.vs",
        .frag = shaders_path / "z_downsample.frag",
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