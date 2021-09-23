#pragma once

#include <variant>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.hpp"
#include "entity.hpp"
#include "model.hpp"
#include "shader.hpp"

namespace engine
{

struct MeshInstance
{
    uint buffer_id;
    int primitive_count;
    int positions_offset;
    int normals_offset;
    int tex_coords_offset;
    int tangents_offset;
};

struct RenderData
{
    Entity::Flags flags;
    size_t mesh_index;
    uint base_color_tex_id;
    uint normal_tex_id;
    uint metallic_roughness_id;
    float metallic_factor;
    float roughness_factor;
    glm::vec3 base_color_factor;
    glm::mat4 model;
    AlphaMode alpha_mode;
    float alpha_cutoff;
    Shader &shader;
};

struct Light
{
    glm::vec3 position;
    glm::vec3 color;
};

struct DirectionalLight
{
    glm::vec3 position;
    glm::vec3 color;
    // Intensity is illuminance at perpendicular incidence in lux.
    float intensity;
    glm::vec3 direction;
};

struct PostProcessingConfig
{
    bool do_tonemap;
    bool do_gamma_correct;
    float exposure;
    float gamma;
};

struct DebugConfig
{
    bool draw_probes;
    bool use_indirect_illumination;
    bool use_direct_illumination;
    bool use_base_color;
    float leak_offset;
};

struct SSAOConfig
{
    int kernel_size;
    int sample_count;
    float radius;
    float bias;
    float strength;
};

struct ShadowMapConfig
{
    bool color_cascades;
    bool stabilize;
    bool filter;
};

struct CameraConfig
{
    float near;
    float far;
};

struct BloomConfig
{
    float threshold;
    float intensity;
    float upsample_radius;
};

struct GBuffer
{
    glm::ivec2 size;
    uint framebuffer;
    uint base_color_roughness;
    uint normal_metallic;
    uint depth;
};

struct IrradianceProbe
{
    glm::vec3 position;
    uint cubemap;
};

class Renderer
{
    enum class Error
    {
        unsupported_texture_format,
    };

    std::array<Light, 3> point_lights = {
        //        Light{glm::vec3{0, 3, 0}, glm::vec3{20.f, 10.f, 10.f}},
        Light{glm::vec3{0, 3, 3}, glm::vec3{0., 0., 0.}},
        Light{glm::vec3{0, 3, 3}, glm::vec3{0., 0., 0.}},
        Light{glm::vec3{3, 3, 3}, glm::vec3{0., 0., 0.}},
    };

    std::vector<MeshInstance> mesh_instances;

    // FIXME:
    unsigned int vao_entities;
    unsigned int vao_skybox;

    Shader shadow_shader;

    Shader skybox_shader;
    unsigned int texture_skybox;

    uint fbo_shadow;
    Shader lighting_shader;

    size_t probe_mesh_idx;

    GBuffer create_g_buffer(glm::ivec2 size);

  public:
    // glm::ivec2 shadow_map_size{2048, 2048};
    glm::ivec2 shadow_map_size{4096, 4096};
    constexpr static int cascade_count = 3;
    std::array<float, cascade_count> cascade_distances;
    std::array<glm::mat4, cascade_count> light_transforms;
    uint shadow_map;
    std::array<uint, cascade_count> debug_view_shadow_maps;

    DirectionalLight sun{
        .position = glm::vec3(0.f, 15., -5.f),
        .color = glm::vec3{1.f, 0.95f, 0.95f},
        .intensity = 40.f,
        .direction = glm::normalize(glm::vec3{0.2f, -1.f, 0.2f}),
    };

    PostProcessingConfig post_proc_cfg{
        .do_tonemap = true,
        .do_gamma_correct = true,
        .exposure = 1.0,
        .gamma = 2.2f,
    };

    DebugConfig debug_cfg{
        .draw_probes = false,
        .use_indirect_illumination = true,
        .use_direct_illumination = true,
        .use_base_color = true,
        .leak_offset = 0.4f,
    };

    SSAOConfig ssao_cfg{
        .kernel_size = 64,
        .sample_count = 64,
        .radius = 0.5f,
        .bias = 0.01f,
        .strength = 2,
    };

    ShadowMapConfig shadow_cfg{
        .color_cascades = false,
        .stabilize = true,
        .filter = true,
    };

    CameraConfig camera_cfg{
        .near = 0.1f,
        .far = 50.f,
    };

    BloomConfig bloom_cfg{
        .threshold = 1.f,
        .intensity = 1.f,
        .upsample_radius = 1.5f,
    };

    // Deferred
    GBuffer g_buf;

    // Debug
    uint debug_view_base_color;
    uint debug_view_roughness;
    uint debug_view_normal;
    uint debug_view_metallic;
    uint debug_view_ssao;

    // GI
    glm::ivec2 probe_env_map_size{64, 64};
    GBuffer g_buf_probe;
    uint fbo_probe;
    uint probe_env_map;
    Shader probe_debug_shader;
    Shader probe_shader;
    std::vector<IrradianceProbe> probes;

    // Post processing
    uint bloom_pass_count = 5;
    uint hdr_target;
    uint hdr_fbo;
    Shader tonemap_shader;

    // Bloom
    uint bloom_downsample_texture;
    uint bloom_upsample_texture;
    Shader bloom_downsample_shader;
    Shader bloom_upsample_shader;

    // SSAO
    uint ssao_fbo;
    std::array<glm::vec3, 64> kernel;
    uint ssao_texture;
    uint ssao_texture_blur;
    uint ssao_noise_texture;
    Shader ssao_shader;
    Shader ssao_blur_shader;

    glm::ivec2 viewport_size;
    Camera camera{glm::vec3{0, 0, 4}, glm::vec3{0}};

    Renderer(glm::ivec2 viewport_size, Shader skybox,
             unsigned int skybox_texture);
    ~Renderer();

    size_t register_mesh(const Mesh &mesh);

    void render_mesh_instance(unsigned int vao, const MeshInstance &mesh);
    void render(std::vector<RenderData> &queue);

    IrradianceProbe generate_probe(glm::vec3 position,
                                   std::vector<RenderData> &queue,
                                   bool use_indirect);
    void generate_probe_grid_gpu(std::vector<RenderData> &queue,
                                 glm::vec3 center, glm::vec3 world_dims,
                                 float distance);
    void generate_probe_grid_cpu(std::vector<RenderData> &queue,
                                 glm::vec3 center, glm::vec3 world_dims,
                                 float distance);

    std::vector<uint> sh_texs;
    glm::mat4 inv_grid_transform;
    glm::vec3 grid_dims;

    void resize_viewport(glm::vec2 size);

    std::variant<uint, Error> register_texture(const Texture &texture);
    void geometry_pass(std::vector<RenderData> &queue, const glm::mat4 &proj,
                       const glm::mat4 &view);
    void lighting_pass(const glm::mat4 &proj, const glm::mat4 &view,
                       const GBuffer &g_buf, bool use_indirect);
    void forward_pass(const glm::mat4 &proj, const glm::mat4 &view);
    void shadow_pass(std::vector<RenderData> &queue, const glm::mat4 &view,
                     float fov, float aspect_ratio);
};

} // namespace engine