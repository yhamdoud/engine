#include <limits>

#include <array>
#include <string>

#include <glm/ext.hpp>
#include <glm/gtx/string_cast.hpp>
#include <stb_image_write.h>

#include "logger.hpp"
#include "model.hpp"
#include "primitives.hpp"
#include "renderer.hpp"

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>
#include <numeric>
#include <random>

using namespace glm;
using namespace std;

using namespace engine;

using std::filesystem::path;

constexpr uint default_framebuffer = 0;

uint binding_positions = 0;
uint binding_normals = 1;
uint binding_tex_coords = 2;
uint binding_tangents = 3;

int attrib_positions = 0;
int attrib_normals = 1;
int attrib_tex_coords = 2;
int attrib_tangents = 3;

void gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                         GLsizei length, GLchar const *message,
                         void const *user_param)
{
    const string source_string{[&source]
                               {
                                   switch (source)
                                   {
                                   case GL_DEBUG_SOURCE_API:
                                       return "API";
                                   case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
                                       return "Window system";
                                   case GL_DEBUG_SOURCE_SHADER_COMPILER:
                                       return "Shader compiler";
                                   case GL_DEBUG_SOURCE_THIRD_PARTY:
                                       return "Third party";
                                   case GL_DEBUG_SOURCE_APPLICATION:
                                       return "Application";
                                   case GL_DEBUG_SOURCE_OTHER:
                                       return "";
                                   }
                               }()};

    const string type_string{[&type]
                             {
                                 switch (type)
                                 {
                                 case GL_DEBUG_TYPE_ERROR:
                                     return "Error";
                                 case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
                                     return "Deprecated behavior";
                                 case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                                     return "Undefined behavior";
                                 case GL_DEBUG_TYPE_PORTABILITY:
                                     return "Portability";
                                 case GL_DEBUG_TYPE_PERFORMANCE:
                                     return "Performance";
                                 case GL_DEBUG_TYPE_MARKER:
                                     return "Marker";
                                 case GL_DEBUG_TYPE_OTHER:
                                     return "";
                                 }
                             }()};

    const auto log_type = [&severity]
    {
        switch (severity)
        {
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return LogType::info;
        case GL_DEBUG_SEVERITY_LOW:
            return LogType::warning;
        case GL_DEBUG_SEVERITY_MEDIUM:
        case GL_DEBUG_SEVERITY_HIGH:
            return LogType::error;
        }
    }();

    logger.log(log_type, "OpenGL {0} {1}: {3}", type_string, source_string, id,
               message);
}

GBuffer Renderer::create_g_buffer(ivec2 size)
{
    uint fbuf, normal_metal, base_color_rough, depth;

    glCreateTextures(GL_TEXTURE_2D, 1, &normal_metal);
    glTextureStorage2D(normal_metal, 1, GL_RGBA16F, size.x, size.y);
    glTextureParameteri(normal_metal, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(normal_metal, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &base_color_rough);
    glTextureStorage2D(base_color_rough, 1, GL_RGBA16F, size.x, size.y);
    glTextureParameteri(base_color_rough, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(base_color_rough, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &depth);
    glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT24, size.x, size.y);
    glTextureParameteri(depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glCreateFramebuffers(1, &fbuf);
    glNamedFramebufferTexture(fbuf, GL_COLOR_ATTACHMENT1, normal_metal, 0);
    glNamedFramebufferTexture(fbuf, GL_COLOR_ATTACHMENT2, base_color_rough, 0);
    glNamedFramebufferTexture(fbuf, GL_DEPTH_ATTACHMENT, depth, 0);

    array<GLenum, 3> draw_bufs{
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
    };
    glNamedFramebufferDrawBuffers(fbuf, 3, draw_bufs.data());

    if (glCheckNamedFramebufferStatus(fbuf, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
        logger.error("G-buffer incomplete");

    return GBuffer{size, fbuf, base_color_rough, normal_metal, depth};
}

Renderer::Renderer(glm::ivec2 viewport_size, Shader skybox,
                   unsigned int skybox_texture)
    : viewport_size{viewport_size}, skybox_shader{skybox}, texture_skybox{
                                                               skybox_texture}
{
    // Enable error callback.
#ifndef ENGINE_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_message_callback, nullptr);
#endif

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_CULL_FACE);

    glClearColor(0.f, 0.f, 0.f, 1.0f);

    // Entity stuff.
    {
        glCreateVertexArrays(1, &vao_entities);

        glEnableVertexArrayAttrib(vao_entities, attrib_positions);
        glVertexArrayAttribFormat(vao_entities, attrib_positions, 3, GL_FLOAT,
                                  false, 0);
        glVertexArrayAttribBinding(vao_entities, attrib_positions,
                                   binding_positions);

        glEnableVertexArrayAttrib(vao_entities, attrib_normals);
        glVertexArrayAttribFormat(vao_entities, attrib_normals, 3, GL_FLOAT,
                                  false, 0);
        glVertexArrayAttribBinding(vao_entities, attrib_normals,
                                   binding_normals);

        glEnableVertexArrayAttrib(vao_entities, attrib_tex_coords);
        glVertexArrayAttribFormat(vao_entities, attrib_tex_coords, 2, GL_FLOAT,
                                  false, 0);
        glVertexArrayAttribBinding(vao_entities, attrib_tex_coords,
                                   binding_tex_coords);

        glEnableVertexArrayAttrib(vao_entities, attrib_tangents);
        glVertexArrayAttribFormat(vao_entities, attrib_tangents, 4, GL_FLOAT,
                                  false, 0);
        glVertexArrayAttribBinding(vao_entities, attrib_tangents,
                                   binding_tangents);
    }

    // Skybox stuff.
    {
        unsigned int buffer;
        glCreateBuffers(1, &buffer);

        glCreateVertexArrays(1, &vao_skybox);

        unsigned int binding = 0, attribute = 0;

        glVertexArrayAttribFormat(vao_skybox, attribute, 3, GL_FLOAT, false, 0);
        glVertexArrayAttribBinding(vao_skybox, attribute, binding);
        glEnableVertexArrayAttrib(vao_skybox, attribute);

        glNamedBufferStorage(buffer, sizeof(primitives::cube_verts),
                             primitives::cube_verts.data(), GL_NONE);
        glVertexArrayVertexBuffer(vao_skybox, binding_positions, buffer, 0,
                                  sizeof(vec3));
    }

    // Shadow mapping stuff.

    shadow_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "shadow_map.vs",
        .geom = shaders_path / "shadow_map.geom",
    });

    // Split scheme. Source:
    // https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
    float lambda = 0.6f;

    const auto &n = camera_cfg.near;
    const auto &f = camera_cfg.far;

    for (int i = 0; i < cascade_count; i++)
    {
        float s_i =
            static_cast<float>(i + 1) / static_cast<float>(cascade_count);
        float log = n * pow(f / n, s_i);
        float uni = n + (f - n) * s_i;
        cascade_distances[i] = lerp(uni, log, lambda);
    }

    // Depth texture rendered from light perspective.
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &shadow_map);

    glTextureParameteri(shadow_map, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadow_map, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadow_map, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(shadow_map, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(shadow_map, GL_TEXTURE_COMPARE_MODE,
                        GL_COMPARE_REF_TO_TEXTURE);

    // Areas beyond the coverage of the shadow map are in light.
    vec4 border_color{1.f};
    glTextureParameterfv(shadow_map, GL_TEXTURE_BORDER_COLOR,
                         value_ptr(border_color));

    glTextureStorage3D(shadow_map, 1, GL_DEPTH_COMPONENT32, shadow_map_size.x,
                       shadow_map_size.y, cascade_count);

    glGenTextures(cascade_count, debug_view_shadow_maps.data());

    for (int i = 0; i < cascade_count; i++)
    {
        glTextureView(debug_view_shadow_maps[i], GL_TEXTURE_2D, shadow_map,
                      GL_DEPTH_COMPONENT32, 0, 1, i, 1);
    }

    glCreateFramebuffers(1, &fbo_shadow);
    glNamedFramebufferTexture(fbo_shadow, GL_DEPTH_ATTACHMENT, shadow_map, 0);
    // We only care about the depth test.
    glNamedFramebufferDrawBuffer(fbo_shadow, GL_NONE);
    glNamedFramebufferReadBuffer(fbo_shadow, GL_NONE);

    g_buf = create_g_buffer(viewport_size);

    array<int, 4> rgb_swizzle{GL_RED, GL_GREEN, GL_BLUE, GL_ONE};
    array<int, 4> www_swizzle{GL_ALPHA, GL_ALPHA, GL_ALPHA, GL_ONE};

    glGenTextures(1, &debug_view_normal);
    glTextureView(debug_view_normal, GL_TEXTURE_2D, g_buf.normal_metallic,
                  GL_RGBA16F, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_normal, GL_TEXTURE_SWIZZLE_RGBA,
                         rgb_swizzle.data());

    glGenTextures(1, &debug_view_metallic);
    glTextureView(debug_view_metallic, GL_TEXTURE_2D, g_buf.normal_metallic,
                  GL_RGBA16F, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_metallic, GL_TEXTURE_SWIZZLE_RGBA,
                         www_swizzle.data());

    glGenTextures(1, &debug_view_base_color);
    glTextureView(debug_view_base_color, GL_TEXTURE_2D,
                  g_buf.base_color_roughness, GL_RGBA8, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_base_color, GL_TEXTURE_SWIZZLE_RGBA,
                         rgb_swizzle.data());

    glGenTextures(1, &debug_view_roughness);
    glTextureView(debug_view_roughness, GL_TEXTURE_2D,
                  g_buf.base_color_roughness, GL_RGBA8, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_roughness, GL_TEXTURE_SWIZZLE_RGBA,
                         www_swizzle.data());

    // Post-processing
    {
        glCreateFramebuffers(1, &hdr_fbo);

        uint depth;
        glCreateRenderbuffers(1, &depth);
        glNamedRenderbufferStorage(depth, GL_DEPTH_COMPONENT24, viewport_size.x,
                                   viewport_size.y);
        glNamedFramebufferRenderbuffer(hdr_fbo, GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER, depth);

        glCreateTextures(GL_TEXTURE_2D, 1, &hdr_target);
        glTextureStorage2D(hdr_target, 1, GL_RGBA16F, viewport_size.x,
                           viewport_size.y);
        glTextureParameteri(hdr_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(hdr_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(hdr_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(hdr_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glNamedFramebufferTexture(hdr_fbo, GL_COLOR_ATTACHMENT0, hdr_target, 0);

        tonemap_shader = *Shader::from_paths(ShaderPaths{
            .vert = shaders_path / "lighting.vs",
            .frag = shaders_path / "tonemap.fs",
        });
    }

    // Bloom
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &bloom_downsample_texture);
        glTextureStorage2D(bloom_downsample_texture, bloom_pass_count,
                           GL_RGBA16F, viewport_size.x / 2,
                           viewport_size.y / 2);
        glTextureParameteri(bloom_downsample_texture, GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR);
        glTextureParameteri(bloom_downsample_texture, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR);
        glTextureParameteri(bloom_downsample_texture, GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
        glTextureParameteri(bloom_downsample_texture, GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);

        glCreateTextures(GL_TEXTURE_2D, 1, &bloom_upsample_texture);
        glTextureStorage2D(bloom_upsample_texture, bloom_pass_count - 1,
                           GL_RGBA16F, viewport_size.x / 2,
                           viewport_size.y / 2);
        glTextureParameteri(bloom_upsample_texture, GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR);
        glTextureParameteri(bloom_upsample_texture, GL_TEXTURE_MIN_FILTER,
                            GL_NEAREST_MIPMAP_LINEAR);
        glTextureParameteri(bloom_upsample_texture, GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
        glTextureParameteri(bloom_upsample_texture, GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);

        bloom_downsample_shader =
            *Shader::from_comp_path(shaders_path / "bloom_downsample.comp");

        bloom_upsample_shader =
            *Shader::from_comp_path(shaders_path / "bloom_upsample.comp");
    }

    // SSAO
    {
        uniform_real_distribution<float> dist1(0.f, 1.f);
        uniform_real_distribution<float> dist2(-1.f, 1.f);
        default_random_engine gen;

        // Generate samples inside and on the unit hemisphere.
        for (int i = 0; i < kernel.size(); i++)
        {
            // We want samples close to the origin, i.e., we care about
            // occlusions close to the fragment.
            float weight =
                static_cast<float>(i) / static_cast<float>(kernel.size());
            // Accelerating interpolation function.
            weight = lerp(0.1f, 1.f, weight * weight);

            kernel[i] =
                weight * normalize(vec3(dist2(gen), dist2(gen), dist1(gen)));
        }

        // Rotation vectors oriented around the tangent-space surface normal.
        constexpr int noise_count = 16;
        array<vec3, noise_count> noise;

        // These vectors are used to rotate the sampling kernel in an effort
        // to minimize banding artifacts.
        for (int i = 0; i < noise_count; i++)
            noise[i] = normalize(vec3(dist2(gen), dist2(gen), 0.f));

        glCreateTextures(GL_TEXTURE_2D, 1, &ssao_noise_texture);
        glTextureStorage2D(ssao_noise_texture, 1, GL_RGBA16F, 4, 4);
        glTextureSubImage2D(ssao_noise_texture, 0, 0, 0, 4, 4, GL_RGB, GL_FLOAT,
                            noise.data());
        glTextureParameteri(ssao_noise_texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(ssao_noise_texture, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glCreateTextures(GL_TEXTURE_2D, 1, &ssao_texture);
        glTextureStorage2D(ssao_texture, 1, GL_R8, viewport_size.x,
                           viewport_size.y);
        glCreateTextures(GL_TEXTURE_2D, 1, &ssao_texture_blur);
        glTextureStorage2D(ssao_texture_blur, 1, GL_R8, viewport_size.x,
                           viewport_size.y);

        uint d;
        glCreateTextures(GL_TEXTURE_2D, 1, &d);
        glTextureStorage2D(d, 1, GL_RGBA32F, viewport_size.x, viewport_size.y);

        glCreateFramebuffers(1, &ssao_fbo);
        glNamedFramebufferTexture(ssao_fbo, GL_COLOR_ATTACHMENT0, ssao_texture,
                                  0);
        glNamedFramebufferTexture(ssao_fbo, GL_COLOR_ATTACHMENT1,
                                  ssao_texture_blur, 0);

        // FIXME:
        glNamedFramebufferTexture(ssao_fbo, GL_COLOR_ATTACHMENT2, d, 0);

        array<GLenum, 3> draw_bufs{
            GL_COLOR_ATTACHMENT0,
            GL_COLOR_ATTACHMENT1,
            GL_COLOR_ATTACHMENT2,
        };
        glNamedFramebufferDrawBuffers(ssao_fbo, 3, draw_bufs.data());

        if (glCheckNamedFramebufferStatus(ssao_fbo, GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE)
            logger.error("SSAO framebuffer incomplete");

        // Clear these so they don't affect probe baking.
        float one = 1.f;
        glClearNamedFramebufferfv(ssao_fbo, GL_COLOR, 0, &one);
        glClearNamedFramebufferfv(ssao_fbo, GL_COLOR, 1, &one);

        array<int, 3> swizzle{GL_RED, GL_RED, GL_RED};

        glGenTextures(1, &debug_view_ssao);
        glTextureView(debug_view_ssao, GL_TEXTURE_2D, ssao_texture, GL_RGB8, 0,
                      1, 0, 1);
        glTextureParameteriv(debug_view_ssao, GL_TEXTURE_SWIZZLE_RGBA,
                             swizzle.data());

        ssao_shader = *Shader::from_paths(ShaderPaths{
            .vert = shaders_path / "lighting.vs",
            .frag = shaders_path / "ssao.fs",
        });

        ssao_blur_shader = *Shader::from_paths(ShaderPaths{
            .vert = shaders_path / "lighting.vs",
            .frag = shaders_path / "ssao_blur.fs",
        });
    }

    // Probe
    {
        g_buf_probe = create_g_buffer(probe_env_map_size);

        glCreateFramebuffers(1, &fbo_probe);

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &probe_env_map);
        glTextureStorage2D(probe_env_map, 1, GL_RGBA16F, probe_env_map_size.x,
                           probe_env_map_size.y);

        uint depth_buffer;
        glCreateRenderbuffers(1, &depth_buffer);
        glNamedRenderbufferStorage(depth_buffer, GL_DEPTH_COMPONENT24,
                                   probe_env_map_size.x, probe_env_map_size.y);
        glNamedFramebufferRenderbuffer(fbo_probe, GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER, depth_buffer);

        if (glCheckNamedFramebufferStatus(fbo_probe, GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE)
            logger.error("Probe framebuffer incomplete");

        probe_shader = *Shader::from_paths(ShaderPaths{
            .vert = shaders_path / "irradiance.vs",
            .frag = shaders_path / "irradiance.fs",
        });

        // Setup state for displaying probe.
        auto sphere_model = std::move(load_gltf(models_path / "sphere.glb")[0]);
        probe_mesh_idx = register_mesh(*sphere_model.mesh);

        probe_debug_shader = *Shader::from_paths(ShaderPaths{
            .vert = shaders_path / "probe.vs",
            .frag = shaders_path / "probe.fs",
        });
    }

    lighting_shader = *Shader::from_paths(ShaderPaths{
        .vert = shaders_path / "lighting.vs",
        .frag = shaders_path / "lighting.fs",
    });
}

Renderer::~Renderer()
{
    // TODO: clean up mesh instances.

    glDeleteVertexArrays(1, &vao_entities);
}

variant<uint, Renderer::Error>
Renderer::register_texture(const Texture &texture)
{
    uint id;
    glCreateTextures(GL_TEXTURE_2D, 1, &id);

    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER,
                        texture.sampler.magnify_filter);
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER,
                        texture.sampler.minify_filter);
    glTextureParameteri(id, GL_TEXTURE_WRAP_S, texture.sampler.wrap_s);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, texture.sampler.wrap_t);

    GLenum internal_format, format;

    switch (texture.component_count)
    {
    case 3:
        internal_format = (texture.sampler.is_srgb) ? GL_SRGB8 : GL_RGB8;
        format = GL_RGB;
        break;
    case 4:
        internal_format = (texture.sampler.is_srgb) ? GL_SRGB8_ALPHA8 : GL_RGB8;
        format = GL_RGBA;
        break;
    default:
        return Renderer::Error::unsupported_texture_format;
    }

    int level_count = 1;

    if (texture.sampler.use_mipmap)
    {
        level_count =
            1 + floor(std::log2(std::max(texture.width, texture.height)));
        glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

    glTextureParameteri(id, GL_TEXTURE_MAX_LEVEL, level_count);
    glTextureParameteri(id, GL_TEXTURE_BASE_LEVEL, 0);

    glTextureStorage2D(id, level_count, internal_format, texture.width,
                       texture.height);
    glTextureSubImage2D(id, 0, 0, 0, texture.width, texture.height, format,
                        GL_UNSIGNED_BYTE, texture.data.get());

    glGenerateTextureMipmap(id);

    return id;
}

size_t Renderer::register_mesh(const Mesh &mesh)
{
    unsigned int id;
    glCreateBuffers(1, &id);

    int primitive_count = mesh.indices.size();

    int size_indices = mesh.indices.size() * sizeof(uint32_t);
    int size_positions = mesh.positions.size() * sizeof(vec3);
    int size_normals = mesh.normals.size() * sizeof(vec3);
    int size_tex_coords = mesh.tex_coords.size() * sizeof(vec2);
    int size_tangents = mesh.tangents.size() * sizeof(vec4);

    // TODO: Investigate if interleaved storage is more efficient than
    // sequential storage for vertex data.
    glNamedBufferStorage(id,
                         size_indices + size_positions + size_normals +
                             size_tex_coords + size_tangents,
                         nullptr, GL_DYNAMIC_STORAGE_BIT);

    MeshInstance instance{.buffer_id = id, .primitive_count = primitive_count};

    int offset = 0;
    glNamedBufferSubData(id, offset, size_indices, mesh.indices.data());
    offset += size_indices;

    glNamedBufferSubData(id, offset, size_positions, mesh.positions.data());
    instance.positions_offset = offset;
    offset += size_positions;

    glNamedBufferSubData(id, offset, size_normals, mesh.normals.data());
    instance.normals_offset = offset;
    offset += size_normals;

    glNamedBufferSubData(id, offset, size_tex_coords, mesh.tex_coords.data());
    instance.tex_coords_offset = offset;
    offset += size_tex_coords;

    glNamedBufferSubData(id, offset, size_tangents, mesh.tangents.data());
    instance.tangents_offset = offset;

    mesh_instances.emplace_back(instance);
    return mesh_instances.size() - 1;
}

void Renderer::render_mesh_instance(unsigned int vao, const MeshInstance &m)
{
    ZoneScoped;

    glVertexArrayElementBuffer(vao, m.buffer_id);
    glVertexArrayVertexBuffer(vao, binding_positions, m.buffer_id,
                              m.positions_offset, sizeof(vec3));
    glVertexArrayVertexBuffer(vao, binding_normals, m.buffer_id,
                              m.normals_offset, sizeof(vec3));
    glVertexArrayVertexBuffer(vao, binding_tex_coords, m.buffer_id,
                              m.tex_coords_offset, sizeof(vec2));
    glVertexArrayVertexBuffer(vao, binding_tangents, m.buffer_id,
                              m.tangents_offset, sizeof(vec4));

    glDrawElements(GL_TRIANGLES, m.primitive_count, GL_UNSIGNED_INT, nullptr);
}

void Renderer::render(std::vector<RenderData> &queue)
{

    const mat4 proj = perspective(radians(90.f),
                                  static_cast<float>(viewport_size.x) /
                                      static_cast<float>(viewport_size.y),
                                  camera_cfg.near, camera_cfg.far);

    const mat4 view = camera.get_view();

    // Shadow mapping pass.
    {
        TracyGpuZone("Shadow mapping");

        glViewport(0, 0, shadow_map_size.x, shadow_map_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_shadow);
        glClear(GL_DEPTH_BUFFER_BIT);

        shadow_pass(queue, view, radians(90.f),
                    static_cast<float>(viewport_size.x) /
                        static_cast<float>(viewport_size.y));
    }

    // Geometry pass
    {
        TracyGpuZone("Geometry pass");

        glViewport(0, 0, viewport_size.x, viewport_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, g_buf.framebuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        geometry_pass(queue, proj, view);
    }

    // SSAO pass
    {
        TracyGpuZone("SSAO pass");

        glViewport(0, 0, viewport_size.x, viewport_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(ssao_shader.get_id());

        ssao_shader.set("u_proj", proj);
        ssao_shader.set("u_proj_inv", inverse(proj));
        ssao_shader.set("u_kernel_size",
                        static_cast<uint>(ssao_cfg.kernel_size));
        ssao_shader.set("u_kernel[0]", span(kernel));
        ssao_shader.set("u_radius", ssao_cfg.radius);
        ssao_shader.set("u_noise_scale",
                        static_cast<vec2>(viewport_size) / 4.f);
        ssao_shader.set("u_bias", ssao_cfg.bias);
        ssao_shader.set("u_strength", ssao_cfg.strength);

        glBindTextureUnit(0, g_buf.depth);
        glBindTextureUnit(1, g_buf.normal_metallic);
        glBindTextureUnit(2, ssao_noise_texture);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glUseProgram(ssao_blur_shader.get_id());

        glBindTextureUnit(0, ssao_texture);

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    {
        TracyGpuZone("Lighting pass");

        glViewport(0, 0, viewport_size.x, viewport_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        lighting_pass(proj, view, g_buf, true);
    }

    //     Render skybox.
    {
        TracyGpuZone("Skybox");

        glViewport(0, 0, viewport_size.x, viewport_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo);
        glBlitNamedFramebuffer(g_buf.framebuffer, hdr_fbo, 0, 0,
                               viewport_size.x, viewport_size.y, 0, 0,
                               viewport_size.x, viewport_size.y,
                               GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        forward_pass(proj, view);
    }

    // Bloom
    {
        TracyGpuZone("Bloom");

        const uint upsample_count = bloom_pass_count - 1;

        glUseProgram(bloom_downsample_shader.get_id());

        bloom_downsample_shader.set("u_threshold", bloom_cfg.threshold);
        glBindTextureUnit(0u, hdr_target);

        for (uint i = 0u; i < bloom_pass_count; i++)
        {
            // Round up.
            const uint group_count_x =
                ((viewport_size.x >> i + 1u) + 15u) / 16u;
            const uint group_count_y =
                ((viewport_size.y >> i + 1u) + 15u) / 16u;

            bloom_downsample_shader.set("u_level", i);

            glBindImageTexture(1u, bloom_downsample_texture, i, false, 0,
                               GL_WRITE_ONLY, GL_RGBA16F);
            glDispatchCompute(group_count_x, group_count_y, 1);
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                            GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            if (i == 0)
            {
                glBindTextureUnit(0u, bloom_downsample_texture);
                bloom_downsample_shader.set("u_threshold", 0.f);
            }
        }

        glUseProgram(bloom_upsample_shader.get_id());

        bloom_upsample_shader.set("u_intensity", 1.f);
        bloom_upsample_shader.set("u_radius", bloom_cfg.upsample_radius);

        glBindTextureUnit(0u, bloom_downsample_texture);
        glBindTextureUnit(1u, bloom_downsample_texture);

        for (int i = upsample_count - 1; i >= 0; i--)
        {
            const uint group_count_x =
                ((viewport_size.x >> i + 1u) + 15u) / 16u;
            const uint group_count_y =
                ((viewport_size.y >> i + 1u) + 15u) / 16u;

            bloom_upsample_shader.set("u_level", (uint)i + 1u);
            bloom_upsample_shader.set("u_target_level", (uint)i);

            glBindImageTexture(2u, bloom_upsample_texture, i, false, 0,
                               GL_WRITE_ONLY, GL_RGBA16F);
            glDispatchCompute(group_count_x, group_count_y, 1u);
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                            GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            if (i == upsample_count - 1)
                glBindTextureUnit(0u, bloom_upsample_texture);
        }

        uint group_count_x = viewport_size.x / 16u;
        uint group_count_y = viewport_size.y / 16u;

        bloom_upsample_shader.set("u_level", 0u);
        bloom_upsample_shader.set("u_target_level", 0u);
        bloom_upsample_shader.set("u_intensity", bloom_cfg.intensity);

        glBindTextureUnit(1u, hdr_target);
        glBindImageTexture(2u, hdr_target, 0, false, 0, GL_READ_WRITE,
                           GL_RGBA16F);

        glDispatchCompute(group_count_x, group_count_y, 1u);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    // Post-processing
    {
        TracyGpuZone("Post-processing");

        glBindFramebuffer(GL_FRAMEBUFFER, default_framebuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(tonemap_shader.get_id());

        tonemap_shader.set("u_hdr_screen", 0);

        tonemap_shader.set("u_do_tonemap", post_proc_cfg.do_tonemap);
        tonemap_shader.set("u_do_gamma_correct",
                           post_proc_cfg.do_gamma_correct);
        tonemap_shader.set("u_exposure", post_proc_cfg.exposure);
        tonemap_shader.set("u_gamma", post_proc_cfg.gamma);

        glBindTextureUnit(0, hdr_target);

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
}

static constexpr vec3 homogenize(const vec4 &v) { return vec3(v) / v.w; }

void Renderer::shadow_pass(vector<RenderData> &queue, const mat4 &view,
                           float fov, float aspect_ratio)
{
    for (int c_idx = 0; c_idx < cascade_count; c_idx++)
    {
        float near = (c_idx == 0) ? 0.1f : cascade_distances[c_idx - 1];
        float far = cascade_distances[c_idx];

        // Camera's projection matrix for the current cascade.
        const mat4 cascade_proj = perspective(fov, aspect_ratio, near, far);

        const mat4 inv = inverse(cascade_proj * view);

        // Camera's frustrum corners in world space.
        const array<vec3, 8> frustrum_corners{
            homogenize(inv * glm::vec4{-1.f, -1.f, -1.f, 1.f}),
            homogenize(inv * glm::vec4{-1.f, -1.f, 1.f, 1.f}),
            homogenize(inv * glm::vec4{-1.f, 1.f, -1.f, 1.f}),
            homogenize(inv * glm::vec4{-1.f, 1.f, 1.f, 1.f}),
            homogenize(inv * glm::vec4{1.f, -1.f, -1.f, 1.f}),
            homogenize(inv * glm::vec4{1.f, -1.f, 1.f, 1.f}),
            homogenize(inv * glm::vec4{1.f, 1.f, -1.f, 1.f}),
            homogenize(inv * glm::vec4{1.f, 1.f, 1.f, 1.f}),
        };

        const vec3 center = accumulate(frustrum_corners.begin(),
                                       frustrum_corners.end(), vec3(0.f)) /
                            8.f;

        vec3 min(numeric_limits<float>::max());
        vec3 max(numeric_limits<float>::min());

        float radius = 0;

        if (shadow_cfg.stabilize)
        {
            // Calculate the radius of the bounding sphere surrounding the
            // frustum.
            // TODO: We could get a tighter fit using something like:
            // https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
            for (const auto &corner : frustrum_corners)
                radius = glm::max(radius, length(corner - center));

            max = vec3(radius);
            min = -max;
        }
        else
        {
            // Construct temporary view matrix.
            mat4 view =
                lookAt(center - sun.direction, center, vec3(0.f, 1.f, 0.f));

            // Transform the frustrum to light space and calculate a tight
            // fitting AABB.
            for (const auto &corner : frustrum_corners)
            {
                const vec3 corner_light_space = view * vec4(corner, 1.f);
                min = glm::min(min, corner_light_space);
                max = glm::max(max, corner_light_space);
            }

            // Include geometry that might be outside the frustrum but
            // contributes to lighting.
            const float z_mult = 15.f;
            const float z_mult_inv = 1 / z_mult;

            min.z *= (min.z < 0) ? z_mult : z_mult_inv;
            max.z *= (max.z < 0) ? z_mult_inv : z_mult;
        }

        mat4 light_view = lookAt(center - sun.direction * -min.z, center,
                                 vec3(0.f, 1.f, 0.f));

        // Snap to shadow map texels.
        if (shadow_cfg.stabilize)
        {
            light_view[3].x -=
                fmodf(light_view[3].x, (radius / shadow_map_size.x) * 2.0f);
            light_view[3].y -=
                fmodf(light_view[3].y, (radius / shadow_map_size.x) * 2.0f);
            // TODO:
            // light_view[3].z -= ...
        }

        mat4 light_proj =
            glm::ortho(min.x, max.y, min.y, max.y, 0.f, max.z - min.z);

        light_transforms[c_idx] = light_proj * light_view;
    }

    glUseProgram(shadow_shader.get_id());

    shadow_shader.set("u_light_transforms[0]", span(light_transforms));

    glBindVertexArray(vao_entities);

    glCullFace(GL_FRONT); // Peter panning.

    for (auto &r : queue)
    {
        if (r.flags & Entity::casts_shadow)
        {
            shadow_shader.set("u_model", r.model);
            render_mesh_instance(vao_entities, mesh_instances[r.mesh_index]);
        }
    }

    glCullFace(GL_BACK);
}

void Renderer::lighting_pass(const mat4 &proj, const mat4 &view,
                             const GBuffer &g_buf, bool use_indirect)
{
    glUseProgram(lighting_shader.get_id());
    lighting_shader.set("u_g_depth", 0);
    lighting_shader.set("u_g_normal_metallic", 1);
    lighting_shader.set("u_g_base_color_roughness", 2);
    lighting_shader.set("u_shadow_map", 3);
    lighting_shader.set("u_ao", 4);
    lighting_shader.set("u_proj_inv", inverse(proj));
    lighting_shader.set("u_view_inv", inverse(view));
    lighting_shader.set("u_use_direct", debug_cfg.use_direct_illumination);
    lighting_shader.set("u_leak_offset", debug_cfg.leak_offset);
    lighting_shader.set("u_use_base_color", debug_cfg.use_base_color);
    lighting_shader.set("u_color_cascades", shadow_cfg.color_cascades);
    lighting_shader.set("u_filter", shadow_cfg.filter);

    glBindTextureUnit(0, g_buf.depth);
    glBindTextureUnit(1, g_buf.normal_metallic);
    glBindTextureUnit(2, g_buf.base_color_roughness);
    glBindTextureUnit(3, shadow_map);
    glBindTextureUnit(4, ssao_texture_blur);

    lighting_shader.set("u_sh_0", 5);
    lighting_shader.set("u_sh_1", 6);
    lighting_shader.set("u_sh_2", 7);
    lighting_shader.set("u_sh_3", 8);
    lighting_shader.set("u_sh_4", 9);
    lighting_shader.set("u_sh_5", 10);
    lighting_shader.set("u_sh_6", 11);

    if (sh_texs.size() == 7 && use_indirect &&
        debug_cfg.use_indirect_illumination)
    {
        lighting_shader.set("u_inv_grid_transform", inv_grid_transform);
        lighting_shader.set("u_grid_dims", grid_dims);

        glBindTextureUnit(5, sh_texs[0]);
        glBindTextureUnit(6, sh_texs[1]);
        glBindTextureUnit(7, sh_texs[2]);
        glBindTextureUnit(8, sh_texs[3]);
        glBindTextureUnit(9, sh_texs[4]);
        glBindTextureUnit(10, sh_texs[5]);
        glBindTextureUnit(11, sh_texs[6]);

        lighting_shader.set("u_use_irradiance", true);
    }
    else
    {
        lighting_shader.set("u_use_irradiance", false);
    }

    // TODO: Use a UBO or SSBO for this data.
    for (size_t i = 0; i < point_lights.size(); i++)
    {
        const auto &light = point_lights[i];
        vec4 pos = view * vec4(light.position, 1);
        lighting_shader.set(fmt::format("u_lights[{}].position", i),
                            vec3(pos) / pos.w);
        lighting_shader.set(fmt::format("u_lights[{}].color", i), light.color);
    }

    lighting_shader.set("u_light_transforms[0]", span(light_transforms));
    lighting_shader.set("u_cascade_distances[0]", span(cascade_distances));

    lighting_shader.set("u_directional_light.direction",
                        vec3{view * vec4{sun.direction, 0.f}});
    lighting_shader.set("u_directional_light.intensity",
                        sun.intensity * sun.color);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::forward_pass(const mat4 &proj, const mat4 &view)
{
    glBindVertexArray(vao_skybox);

    glUseProgram(skybox_shader.get_id());

    skybox_shader.set("u_projection", proj);
    skybox_shader.set("u_view", mat4(mat3(view)));

    glBindTextureUnit(0, texture_skybox);

    glDrawArrays(GL_TRIANGLES, 0, 36);

    if (debug_cfg.draw_probes)
    {
        glBindVertexArray(vao_entities);

        glUseProgram(probe_debug_shader.get_id());

        float far_clip_dist = proj[3][2] / (proj[2][2] + 1.f);
        probe_debug_shader.set("u_far_clip_distance", far_clip_dist);

        probe_debug_shader.set("u_inv_grid_transform", inv_grid_transform);
        probe_debug_shader.set("u_grid_dims", grid_dims);

        // FIXME:
        if (sh_texs.size() == 0)
            return;

        probe_debug_shader.set("u_sh_0", 0);
        probe_debug_shader.set("u_sh_1", 1);
        probe_debug_shader.set("u_sh_2", 2);
        probe_debug_shader.set("u_sh_3", 3);
        probe_debug_shader.set("u_sh_4", 4);
        probe_debug_shader.set("u_sh_5", 5);
        probe_debug_shader.set("u_sh_6", 6);

        glBindTextureUnit(0, sh_texs[0]);
        glBindTextureUnit(1, sh_texs[1]);
        glBindTextureUnit(2, sh_texs[2]);
        glBindTextureUnit(3, sh_texs[3]);
        glBindTextureUnit(4, sh_texs[4]);
        glBindTextureUnit(5, sh_texs[5]);
        glBindTextureUnit(6, sh_texs[6]);

        for (const auto &probe : probes)
        {
            const mat4 model =
                scale(translate(mat4(1.f), probe.position), vec3(0.2f));
            const mat4 model_view = view * model;

            probe_debug_shader.set("u_model", model);
            probe_debug_shader.set("u_model_view", model_view);
            probe_debug_shader.set("u_mvp", proj * model_view);

            render_mesh_instance(vao_entities, mesh_instances[probe_mesh_idx]);
        }
    }
}

void Renderer::geometry_pass(vector<RenderData> &queue, const mat4 &proj,
                             const mat4 &view)
{
    glBindVertexArray(vao_entities);

    uint cur_shader_id = invalid_shader_id;

    for (auto &r : queue)
    {
        if (auto shader_id = r.shader.get_id(); cur_shader_id != shader_id)
        {
            glUseProgram(shader_id);
            cur_shader_id = shader_id;
        }

        const auto model_view = view * r.model;
        const auto mvp = proj * model_view;

        r.shader.set("u_model_view", model_view);
        r.shader.set("u_mvp", mvp);
        r.shader.set("u_normal_mat", inverseTranspose(mat3{model_view}));
        float far_clip_dist = proj[3][2] / (proj[2][2] + 1.f);
        r.shader.set("u_far_clip_distance", far_clip_dist);

        r.shader.set("u_base_color_factor", r.base_color_factor);
        r.shader.set("u_metallic_factor", r.metallic_factor);
        r.shader.set("u_roughness_factor", r.roughness_factor);
        r.shader.set("u_alpha_mask", r.alpha_mode == AlphaMode::mask);
        r.shader.set("u_alpha_cutoff", r.alpha_cutoff);

        if (r.base_color_tex_id != invalid_texture_id)
        {
            r.shader.set("u_use_sampler", true);
            r.shader.set("u_base_color", 0);
            glBindTextureUnit(0, r.base_color_tex_id);
        }
        else
        {
            r.shader.set("u_use_sampler", false);
        }

        if (r.normal_tex_id != invalid_texture_id)
        {
            r.shader.set("u_use_normal", true);
            r.shader.set("u_normal", 1);
            glBindTextureUnit(1, r.normal_tex_id);
        }
        else
        {
            r.shader.set("u_use_normal", false);
        }

        if (r.metallic_roughness_id != invalid_texture_id)
        {
            r.shader.set("u_use_metallic_roughness", true);
            r.shader.set("u_metallic_roughness", 2);
            glBindTextureUnit(2, r.metallic_roughness_id);
        }
        else
        {
            r.shader.set("u_use_metallic_roughness", false);
        }

        render_mesh_instance(vao_entities, mesh_instances[r.mesh_index]);
    }
}

void Renderer::resize_viewport(glm::vec2 size)
{
    viewport_size = size;
    // TODO: clean up buffers
    g_buf = create_g_buffer(size);
}

IrradianceProbe Renderer::generate_probe(vec3 position,
                                         std::vector<RenderData> &queue,
                                         bool use_indirect)
{
    IrradianceProbe probe{position, invalid_texture_id};

    float fov = radians(90.f);
    mat4 proj = perspective(fov, 1.f, 0.1f, 50.f);
    array<mat4, 6> views{
        lookAt(position, position + vec3{1.f, 0.f, 0.f}, vec3{0.f, -1.f, 0.f}),
        lookAt(position, position + vec3{-1.f, 0.f, 0.f}, vec3{0.f, -1.f, 0.f}),
        lookAt(position, position + vec3{0.f, 1.f, 0.f}, vec3{0.f, 0.f, 1.f}),
        lookAt(position, position + vec3{0.f, -1.f, 0.f}, vec3{0.f, 0.f, -1.f}),
        lookAt(position, position + vec3{0.f, 0.f, 1.f}, vec3{0.f, -1.f, 0.f}),
        lookAt(position, position + vec3{0.f, 0.f, -1.f}, vec3{0.f, -1.f, 0.f}),
    };

    for (int face_idx = 0; face_idx < 6; face_idx++)
    {
        glViewport(0, 0, shadow_map_size.x, shadow_map_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_shadow);
        glClear(GL_DEPTH_BUFFER_BIT);
        shadow_pass(queue, views[face_idx], fov, 1.f);

        glViewport(0, 0, probe_env_map_size.x, probe_env_map_size.y);
        glNamedFramebufferTextureLayer(fbo_probe, GL_COLOR_ATTACHMENT0,
                                       probe_env_map, 0, face_idx);

        glBindFramebuffer(GL_FRAMEBUFFER, g_buf_probe.framebuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        geometry_pass(queue, proj, views[face_idx]);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_probe);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        lighting_pass(proj, views[face_idx], g_buf_probe, use_indirect);
        glBlitNamedFramebuffer(g_buf_probe.framebuffer, fbo_probe, 0, 0,
                               probe_env_map_size.x, probe_env_map_size.y, 0, 0,
                               probe_env_map_size.x, probe_env_map_size.y,
                               GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        forward_pass(proj, views[face_idx]);
    }

    probe.cubemap = probe_env_map;
    return probe;

    //    ivec2 cubemap_size{32, 32};
    //    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &probe.cubemap);
    //    glTextureStorage2D(probe.cubemap, 1, GL_RGBA16F, cubemap_size.x,
    //                       cubemap_size.y);
    //
    //    glTextureParameteri(probe.cubemap, GL_TEXTURE_WRAP_S,
    //    GL_CLAMP_TO_EDGE); glTextureParameteri(probe.cubemap,
    //    GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //    glTextureParameteri(probe.cubemap, GL_TEXTURE_WRAP_R,
    //    GL_CLAMP_TO_EDGE); glTextureParameteri(probe.cubemap,
    //    GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //    glTextureParameteri(probe.cubemap, GL_TEXTURE_MAG_FILTER,
    //    GL_LINEAR);
    //
    //    glUseProgram(probe_shader.get_id());
    //
    //    glBindTextureUnit(0, probe_env_map);
    //
    //    probe_shader.set("u_projection", proj);
    //    probe_shader.set("u_environment", 0);
    //
    //    glBindVertexArray(vao_skybox);
    //
    //    glViewport(0, 0, cubemap_size.x, cubemap_size.y);
    //
    //    for (int face_idx = 0; face_idx < 6; face_idx++)
    //    {
    //        glNamedFramebufferTextureLayer(fbo_probe,
    //        GL_COLOR_ATTACHMENT0,
    //                                       probe.cubemap, 0, face_idx);
    //        glBindFramebuffer(GL_FRAMEBUFFER, fbo_probe);
    //        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //
    //        probe_shader.set("u_view", mat4(mat3(views[face_idx])));
    //        glDrawArrays(GL_TRIANGLES, 0, 36);
    //    }
    //
    //    return probe;
}

enum CubeMapFace
{
    positive_x,
    negative_x,
    positive_y,
    negative_y,
    positive_z,
    negative_z,
};

// Real valued coefficients of first 3 bands of SH functions sampled at a
// given point on the unit sphere.
struct SphericalHarmonics3
{
    array<float, 9> coefficients;

    // Evaluation of the SH basis functions at the given point.
    explicit SphericalHarmonics3(const vec3 &s)
        : coefficients{
              // Band 0
              0.282095f,
              // Band 1
              0.488603f * s.y,
              0.488603f * s.z,
              0.488603f * s.x,
              // Band 2
              1.092548f * s.x * s.y,
              1.092548f * s.y * s.z,
              0.315392f * (3.0f * s.z * s.z - 1.0f),
              1.092548f * s.x * s.z,
              0.546274f * (s.x * s.x - s.y * s.y),
          }
    {
    }
};

// Convert a texture coordinate and cube map face to a vector directed to
// the corresponding texel in world space.
vec3 cube_map_st_to_dir(vec2 coords, CubeMapFace face)
{
    // Map [0, 1] to [-1, 1].
    coords = 2.f * coords - 1.f;

    // https://www.khronos.org/opengl/wiki/Cubemap_Texture#Upload_and_orientation
    switch (face)
    {
    case CubeMapFace::positive_x:
        return vec3{1.0f, -coords.t, -coords.s};
    case CubeMapFace::negative_x:
        return vec3{-1.0f, -coords.t, coords.s};
    case CubeMapFace::positive_y:
        return vec3{coords.s, 1.0f, coords.t};
    case CubeMapFace::negative_y:
        return vec3{coords.s, -1.0f, -coords.t};
    case CubeMapFace::positive_z:
        return vec3{coords.s, -coords.t, 1.0f};
    case CubeMapFace::negative_z:
        return vec3{coords.s, -coords.t, -1.0f};
    }
}

void sh_reconstruct(array<vec3, 9> sh, int size)
{
    const int face_size = size * size;
    const int face_count = 6;

    vector<glm::vec3> texels(face_size * face_count);

    int face_idx = 0;

    for (int face_idx = 0; face_idx < face_count; face_idx++)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                vec2 coords =
                    (vec2(x, y) + vec2(0.5f)) / static_cast<float>(size);
                // Project cubemap texel position on a sphere.
                vec3 s = normalize(
                    cube_map_st_to_dir(coords, (CubeMapFace)face_idx));

                // clang-format off
                texels.at(x + y * size + face_idx * face_size) =
                    sh[0] * 0.282095f +
                    sh[1] * 0.488603f * s.y +
                    sh[2] * 0.488603f * s.z +
                    sh[3] * 0.488603f * s.x;
                    sh[4] * 1.092548f * s.x * s.y +
                    sh[5] * 1.092548f * s.y * s.z +
                    sh[6] * 0.315392f * (3.0f * s.z * s.z - 1.0f) +
                    sh[7] * 1.092548f * s.x * s.z +
                    sh[8] * 0.546274f * (s.x * s.x - s.y * s.y);
                // clang-format on
            }
        }
    }

    stbi_write_hdr("radiance.hdr", size, face_count * size, 3,
                   reinterpret_cast<float *>(texels.data()));
}

array<vec3, 9> sh_project(uint cube_map, int size)
{
    const int face_size = size * size;
    const int face_count = 6;

    vector<glm::vec3> texels(face_count * face_size);
    glGetTextureImage(cube_map, 0, GL_RGB, GL_FLOAT,
                      texels.size() * sizeof(vec3), texels.data());

    array<vec3, 9> rgb_coeffs{};
    float weight_sum = 0;

    for (int face_idx = 0; face_idx < face_count; face_idx++)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                const vec3 &texel =
                    texels.at(x + y * size + face_idx * face_size);
                vec2 coords =
                    (vec2{x, y} + vec2(0.5f)) / static_cast<float>(size);
                // Project cubemap texel position on a sphere.
                vec3 s = normalize(
                    cube_map_st_to_dir(coords, (CubeMapFace)face_idx));

                // Differential solid angle.
                coords.s = coords.s * 2.f - 1.f;
                coords.t = coords.t * 2.f - 1.f;
                float tmp = 1 + coords.s * coords.s + coords.t * coords.t;
                float weight = 4.f / (sqrt(tmp) * tmp);

                // Evaluate the SH functions at center of the current texel.
                SphericalHarmonics3 sh(s);

                for (int i = 0; i < 9; i++)
                    rgb_coeffs[i] += weight * texel * sh.coefficients[i];

                weight_sum += weight;
            }
        }
    }

    for (auto &c : rgb_coeffs)
        c *= 4.f * pi<float>() / weight_sum;

    return rgb_coeffs;
}

void Renderer::generate_probe_grid_cpu(std::vector<RenderData> &queue,
                                       glm::vec3 center, glm::vec3 world_dims,
                                       float distance)
{
    logger.info("Generating probe grid");

    ivec3 dims = world_dims / distance;
    int probe_count = dims.x * dims.y * dims.z;
    // We can pack the 27 coefficient into 7 RGBA textures
    int texture_count = 7;

    vec3 origin = center - world_dims / 2.f;

    mat4 grid_transform = scale(translate(mat4(1.f), origin), vec3(distance));

    this->grid_dims = dims;
    this->inv_grid_transform = inverse(grid_transform);

    logger.debug("{}", glm::to_string(inv_grid_transform * vec4(origin, 1.f)));

    logger.debug("{}", glm::to_string(dims));

    // Contigious storage for 3D textures, each containing the per channel
    // coefficients of a single SH band.
    vector<vec4> coeffs(probe_count * texture_count);

    sh_texs.resize(texture_count);
    glCreateTextures(GL_TEXTURE_3D, texture_count, sh_texs.data());

    int bounce_count = 4;

    for (int i = 0; i < bounce_count; i++)
    {
        for (int z = 0; z < dims.z; z++)
        {
            for (int y = 0; y < dims.y; y++)
            {
                for (int x = 0; x < dims.x; x++)
                {
                    vec3 position = vec3(grid_transform * vec4{x, y, z, 1});

                    // Rasterize probe environment map.
                    auto probe = generate_probe(position, queue, i != 0);
                    if (i == 0)
                        probes.emplace_back(probe);

                    // Project environment cube map on the SH basis.
                    auto cs = sh_project(probe_env_map, probe_env_map_size.x);

                    int idx = x + y * dims.x + z * dims.x * dims.y;

                    coeffs.at(idx) = vec4(cs[0], cs[7][0]);
                    coeffs.at(idx + 1 * probe_count) = vec4(cs[1], cs[7][1]);
                    coeffs.at(idx + 2 * probe_count) = vec4(cs[2], cs[7][2]);
                    coeffs.at(idx + 3 * probe_count) = vec4(cs[3], cs[8][0]);
                    coeffs.at(idx + 4 * probe_count) = vec4(cs[4], cs[8][1]);
                    coeffs.at(idx + 5 * probe_count) = vec4(cs[5], cs[8][2]);
                    coeffs.at(idx + 6 * probe_count) = vec4(cs[6], 0);
                }
            }
        }

        for (int tex_idx = 0; tex_idx < texture_count; tex_idx++)
        {
            if (i == 0)
            {
                glTextureStorage3D(sh_texs[tex_idx], 1, GL_RGBA16F, dims.x,
                                   dims.y, dims.z);
            }

            glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_WRAP_S,
                                GL_CLAMP_TO_EDGE);
            glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_WRAP_T,
                                GL_CLAMP_TO_EDGE);
            glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_WRAP_R,
                                GL_CLAMP_TO_EDGE);

            // Important for trilinear interpolation!
            glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_MAG_FILTER,
                                GL_LINEAR);
            glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_LINEAR);

            glTextureSubImage3D(sh_texs[tex_idx], 0, 0, 0, 0, dims.x, dims.y,
                                dims.z, GL_RGBA, GL_FLOAT,
                                &coeffs.at(tex_idx * probe_count));
        }
    }

    logger.info("Probe grid generation finished");
}

void Renderer::generate_probe_grid_gpu(std::vector<RenderData> &queue,
                                       glm::vec3 center, glm::vec3 world_dims,
                                       float distance)
{
    ivec3 dims = world_dims / distance;
    int probe_count = dims.x * dims.y * dims.z;

    logger.info("Baking {} probes", probe_count);

    // We can pack the 27 coefficient into 7 4 channel textures
    const int texture_count = 7;
    const ivec2 local_size{16, 16};
    const ivec3 group_count{probe_env_map_size / local_size, 6};

    const vec3 origin = center - world_dims / 2.f;

    const mat4 grid_transform =
        scale(translate(mat4(1.f), origin), vec3(distance));
    this->inv_grid_transform = inverse(grid_transform);

    this->grid_dims = dims;

    sh_texs.resize(texture_count);
    glCreateTextures(GL_TEXTURE_3D, texture_count, sh_texs.data());

    auto project = *Shader::from_comp_path(shaders_path / "sh_project.comp");
    auto reduce = *Shader::from_comp_path(shaders_path / "sh_reduce.comp");

    for (int tex_idx = 0; tex_idx < texture_count; tex_idx++)
    {
        glTextureStorage3D(sh_texs[tex_idx], 1, GL_RGBA16F, dims.x, dims.y,
                           dims.z);

        glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
        glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);
        glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_WRAP_R,
                            GL_CLAMP_TO_EDGE);

        // Important for trilinear interpolation!
        glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(sh_texs[tex_idx], GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR);
    }

    const int byte_size = group_count.x * group_count.y * group_count.z *
                          texture_count * sizeof(vec4);

    uint ssbo;
    glCreateBuffers(1, &ssbo);
    glNamedBufferStorage(ssbo, byte_size, nullptr, GL_NONE);

    // Compute the final weight for integration
    float weight_sum = 0.f;
    for (int y = 0; y < probe_env_map_size.x; y++)
    {
        for (int x = 0; x < probe_env_map_size.y; x++)
        {
            const vec2 coords = ((vec2(x, y) + vec2(0.5f)) /
                                 static_cast<vec2>(probe_env_map_size)) *
                                    2.0f -
                                1.0f;

            const float tmp = 1.f + coords.s * coords.s + coords.t * coords.t;
            weight_sum += 4.f / (sqrt(tmp) * tmp);
        }
    }

    weight_sum = 4.f * pi<float>() / weight_sum;

    int bounce_count = 3;

    for (int i = 0; i < bounce_count; i++)
    {
        for (int z = 0; z < dims.z; z++)
        {
            for (int y = 0; y < dims.y; y++)
            {
                for (int x = 0; x < dims.x; x++)
                {
                    vec3 position = vec3(grid_transform * vec4{x, y, z, 1});
                    // Rasterize probe environment map.
                    auto probe = generate_probe(position, queue, i != 0);
                    // FIXME
                    if (i == 0)
                        probes.emplace_back(probe);

                    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                                    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                    glUseProgram(project.get_id());
                    project.set("u_weight_sum", weight_sum);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
                    glBindImageTexture(0, probe_env_map, 0, true, 0,
                                       GL_READ_ONLY, GL_RGBA16F);

                    glDispatchCompute(group_count.x, group_count.y,
                                      group_count.z);

                    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                    glUseProgram(reduce.get_id());
                    reduce.set("u_idx", ivec3{x, y, z});
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, ssbo);
                    for (int j = 0; j < texture_count; j++)
                        glBindImageTexture(j, sh_texs[j], 0, true, 0,
                                           GL_WRITE_ONLY, GL_RGBA16F);

                    glDispatchCompute(1, 1, 1);
                }
            }
        }

        logger.info("Bounce {} complete.", i);
    }

    logger.info("Probe grid generation finished");
}
