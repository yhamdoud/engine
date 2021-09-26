#pragma once

#include "../constants.hpp"
#include "../pass.hpp"
#include "../render_context.hpp"
#include "../shader.hpp"

namespace engine
{

struct GeometryConfig
{
};

class GeometryPass
{
    uint f_buf, normal_metal, base_color_rough, depth;

  public:
    GeometryPass();

    uint debug_view_metallic, debug_view_normal, debug_view_base_color,
        debug_view_roughness;

    void create_debug_views();
    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx, RenderContext &r_ctx);
};

static_assert(Pass<GeometryPass>);

} // namespace engine