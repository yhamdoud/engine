#pragma once

#include "../constants.hpp"
#include "../render_context.hpp"
#include "../shader.hpp"

namespace engine
{

struct ShadowConfig
{
    glm::ivec2 size;
    bool stabilize;
};

class ShadowPass
{
    constexpr static int cascade_count = 3;

    uint frame_buf;
    Shader shader;
    uint shadow_map;

    std::array<float, cascade_count> cascade_distances;
    std::array<glm::mat4, cascade_count> light_transforms;

  public:
    bool stabilize;

    glm::ivec2 size{4096, 4096};

    std::array<uint, cascade_count> debug_views;

    ShadowPass(ShadowConfig cfg);

    void parse_parameters();
    void initialize(ViewportContext &ctx);
    void render(ViewportContext &ctx, RenderContext &ctr_r);
};

} // namespace engine