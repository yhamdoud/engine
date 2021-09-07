#include <array>
#include <string>

#include <glm/ext.hpp>
#include <stb_image_write.h>

#include "logger.hpp"
#include "primitives.hpp"
#include "renderer.hpp"

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>

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

void Renderer::create_g_buffer()
{
    glCreateFramebuffers(1, &g_buffer);

    glCreateTextures(GL_TEXTURE_2D, 1, &g_normal_metallic);
    glTextureStorage2D(g_normal_metallic, 1, GL_RGBA16F, viewport_size.x,
                       viewport_size.y);
    glTextureSubImage2D(g_normal_metallic, 0, 0, 0, viewport_size.x,
                        viewport_size.y, GL_RGBA, GL_FLOAT, nullptr);
    glTextureParameteri(g_normal_metallic, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(g_normal_metallic, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glNamedFramebufferTexture(g_buffer, GL_COLOR_ATTACHMENT1, g_normal_metallic,
                              0);

    glCreateTextures(GL_TEXTURE_2D, 1, &g_base_color_roughness);
    glTextureStorage2D(g_base_color_roughness, 1, GL_RGBA8, viewport_size.x,
                       viewport_size.y);
    glTextureSubImage2D(g_base_color_roughness, 0, 0, 0, viewport_size.x,
                        viewport_size.y, GL_RGBA, GL_FLOAT, nullptr);
    glTextureParameteri(g_base_color_roughness, GL_TEXTURE_MIN_FILTER,
                        GL_NEAREST);
    glTextureParameteri(g_base_color_roughness, GL_TEXTURE_MAG_FILTER,
                        GL_NEAREST);
    glNamedFramebufferTexture(g_buffer, GL_COLOR_ATTACHMENT2,
                              g_base_color_roughness, 0);

    glCreateTextures(GL_TEXTURE_2D, 1, &g_depth);
    glTextureStorage2D(g_depth, 1, GL_DEPTH_COMPONENT24, viewport_size.x,
                       viewport_size.y);
    glTextureSubImage2D(g_depth, 0, 0, 0, viewport_size.x, viewport_size.y,
                        GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTextureParameteri(g_depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(g_depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glNamedFramebufferTexture(g_buffer, GL_DEPTH_ATTACHMENT, g_depth, 0);

    array<GLenum, 3> bufs{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                          GL_COLOR_ATTACHMENT2};
    glNamedFramebufferDrawBuffers(g_buffer, 3, bufs.data());

    if (glCheckNamedFramebufferStatus(g_buffer, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
        logger.error("G-buffer incomplete");

    array<int, 4> rgb_swizzle{GL_RED, GL_GREEN, GL_BLUE, GL_ONE};
    array<int, 4> www_swizzle{GL_ALPHA, GL_ALPHA, GL_ALPHA, GL_ONE};

    glGenTextures(1, &debug_view_normal);
    glTextureView(debug_view_normal, GL_TEXTURE_2D, g_normal_metallic,
                  GL_RGBA16F, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_normal, GL_TEXTURE_SWIZZLE_RGBA,
                         rgb_swizzle.data());

    glGenTextures(1, &debug_view_metallic);
    glTextureView(debug_view_metallic, GL_TEXTURE_2D, g_normal_metallic,
                  GL_RGBA16F, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_metallic, GL_TEXTURE_SWIZZLE_RGBA,
                         www_swizzle.data());

    glGenTextures(1, &debug_view_base_color);
    glTextureView(debug_view_base_color, GL_TEXTURE_2D, g_base_color_roughness,
                  GL_RGBA8, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_base_color, GL_TEXTURE_SWIZZLE_RGBA,
                         rgb_swizzle.data());

    glGenTextures(1, &debug_view_roughness);
    glTextureView(debug_view_roughness, GL_TEXTURE_2D, g_base_color_roughness,
                  GL_RGBA8, 0, 1, 0, 1);
    glTextureParameteriv(debug_view_roughness, GL_TEXTURE_SWIZZLE_RGBA,
                         www_swizzle.data());
}

Renderer::Renderer(glm::ivec2 viewport_size, Shader skybox,
                   unsigned int skybox_texture)
    : viewport_size{viewport_size}, skybox_shader{skybox}, texture_skybox{
                                                               skybox_texture}
{
    // Enable error callback.
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_message_callback, nullptr);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

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

    shadow_shader = *Shader::from_paths(
        ShaderPaths{.vert = shaders_path / "shadow_map.vs"});

    // Depth texture rendered from light perspective.
    glCreateTextures(GL_TEXTURE_2D, 1, &shadow_map);

    glTextureParameteri(shadow_map, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadow_map, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadow_map, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(shadow_map, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Areas beyond the coverage of the shadow map are in light.
    vec4 border_color{1.f};
    glTextureParameterfv(shadow_map, GL_TEXTURE_BORDER_COLOR,
                         value_ptr(border_color));

    // TODO: internal format.
    glTextureStorage2D(shadow_map, 1, GL_DEPTH_COMPONENT24, shadow_map_size.x,
                       shadow_map_size.y);
    glTextureSubImage2D(shadow_map, 0, 0, 0, shadow_map_size.x,
                        shadow_map_size.y, GL_DEPTH_COMPONENT, GL_FLOAT,
                        nullptr);

    glCreateFramebuffers(1, &fbo_shadow);
    glNamedFramebufferTexture(fbo_shadow, GL_DEPTH_ATTACHMENT, shadow_map, 0);
    // We only care about the depth test.
    glNamedFramebufferDrawBuffer(fbo_shadow, GL_NONE);
    glNamedFramebufferReadBuffer(fbo_shadow, GL_NONE);

    create_g_buffer();

    // Post-processing
    {
        glCreateFramebuffers(1, &hdr_fbo);

        uint depth;
        glCreateRenderbuffers(1, &depth);
        glNamedRenderbufferStorage(depth, GL_DEPTH_COMPONENT24, viewport_size.x,
                                   viewport_size.y);
        glNamedFramebufferRenderbuffer(hdr_fbo, GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER, depth);

        glCreateTextures(GL_TEXTURE_2D, 1, &hdr_screen);
        glTextureStorage2D(hdr_screen, 1, GL_RGBA16F, viewport_size.x,
                           viewport_size.y);
        glTextureSubImage2D(hdr_screen, 0, 0, 0, viewport_size.x,
                            viewport_size.y, GL_RGBA, GL_FLOAT, nullptr);
        glTextureParameteri(hdr_screen, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(hdr_screen, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glNamedFramebufferTexture(hdr_fbo, GL_COLOR_ATTACHMENT0, hdr_screen, 0);

        tonemap_shader = *Shader::from_paths(ShaderPaths{
            .vert = shaders_path / "lighting.vs",
            .frag = shaders_path / "tonemap.fs",
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
                                  0.1f, far_clip_distance);

    const mat4 view = camera.get_view();

    const mat4 light_transform =
        ortho(-20.f, 20.f, -20.f, 20.f, 1.f, 20.f) *
        lookAt(sun.position, sun.position + sun.direction, vec3{0.f, 1.f, 0.f});

    // Shadow mapping pass.
    {
        TracyGpuZone("Shadow mapping");

        glCullFace(GL_FRONT);

        glViewport(0, 0, shadow_map_size.x, shadow_map_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_shadow);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Directional light.
        glUseProgram(shadow_shader.get_id());
        shadow_shader.set("u_light_transform", light_transform);

        glBindVertexArray(vao_entities);

        for (auto &r : queue)
        {
            if (r.flags & Entity::casts_shadow)
            {

                shadow_shader.set("u_model", r.model);
                render_mesh_instance(vao_entities,
                                     mesh_instances[r.mesh_index]);
            }
        }

        glCullFace(GL_BACK);
    }

    // Geometry pass
    {
        TracyGpuZone("Geometry pass");

        glViewport(0, 0, viewport_size.x, viewport_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, g_buffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
            r.shader.set("u_far_clip_distance", far_clip_distance);

            r.shader.set("u_metallic_factor", r.metallic_factor);
            r.shader.set("u_roughness_factor", r.roughness_factor);

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

    {
        TracyGpuZone("Lighting pass");

        glViewport(0, 0, viewport_size.x, viewport_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(lighting_shader.get_id());
        lighting_shader.set("u_g_depth", 0);
        lighting_shader.set("u_g_normal_metallic", 1);
        lighting_shader.set("u_g_base_color_roughness", 2);
        lighting_shader.set("u_shadow_map", 3);
        lighting_shader.set("u_proj_inv", inverse(proj));
        lighting_shader.set("u_view_inv", inverse(view));

        glBindTextureUnit(0, g_depth);
        glBindTextureUnit(1, g_normal_metallic);
        glBindTextureUnit(2, g_base_color_roughness);
        glBindTextureUnit(3, shadow_map);

        // TODO: Use a UBO or SSBO for this data.
        for (size_t i = 0; i < point_lights.size(); i++)
        {
            const auto &light = point_lights[i];
            vec4 pos = view * vec4(light.position, 1);
            lighting_shader.set(fmt::format("u_lights[{}].position", i),
                                vec3(pos) / pos.w);
            lighting_shader.set(fmt::format("u_lights[{}].color", i),
                                light.color);
        }

        lighting_shader.set("u_light_transform", light_transform);
        lighting_shader.set("u_directional_light.direction",
                            vec3{view * vec4{sun.direction, 0.f}});
        lighting_shader.set("u_directional_light.intensity",
                            sun.intensity * sun.color);

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    //     Render skybox.
    {
        TracyGpuZone("Skybox");

        glBlitNamedFramebuffer(
            g_buffer, hdr_fbo, 0, 0, viewport_size.x, viewport_size.y, 0, 0,
            viewport_size.x, viewport_size.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo);

        glUseProgram(skybox_shader.get_id());
        glBindTextureUnit(0, texture_skybox);

        skybox_shader.set("u_projection", proj);
        skybox_shader.set("u_view", mat4(mat3(view)));

        glBindVertexArray(vao_skybox);
        glDrawArrays(GL_TRIANGLES, 0, 36);
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

        glBindTextureUnit(0, hdr_screen);

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
}
void Renderer::resize_viewport(glm::vec2 size)
{
    viewport_size = size;
    create_g_buffer();
}
