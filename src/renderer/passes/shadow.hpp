#pragma once

#include "renderer/context.hpp"
#include "renderer/pass.hpp"

namespace engine
{

class ShadowPass
{
    static constexpr int max_cascade_count = 5;

    struct Params
    {
        glm::ivec2 size;
        int cascade_count;
        bool stabilize;
        float z_multiplier;
        bool cull_front_faces;
        bool render_point_lights;
    };

    uint frame_buf;

    Shader directional_shader;
    Shader omni_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "shadow_omni.vert",
        .frag = shaders_path / "shadow_omni.frag",
    });
    uint shadow_map;

    std::array<float, max_cascade_count> cascade_distances;
    std::array<glm::mat4, max_cascade_count> light_transforms;

  public:
    Params params;

    std::array<uint, max_cascade_count> debug_views;

    ShadowPass(Params params);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx, RenderContext &ctr_r);
};

} // namespace engine