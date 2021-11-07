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
    struct RenderArgs
    {
        glm::vec2 size{};
        uint framebuf = default_frame_buffer_id;
        glm::mat4 view{};
        glm::mat4 view_proj{};
        glm::mat4 view_proj_prev{};
        uint32_t entity_vao = 0;
        MeshInstance &sphere_mesh;
        std::vector<Entity> &entities;
        std::vector<MeshInstance> &meshes;
        std::vector<Light> &lights;
        glm::vec2 jitter{};
        glm::vec2 jitter_prev{};
    };

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
    void render(const RenderArgs &args);
};

} // namespace engine