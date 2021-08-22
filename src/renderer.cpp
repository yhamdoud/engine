#include <iostream>
#include <string>

#include <glm/ext.hpp>

#include "primitives.hpp"
#include "renderer.hpp"

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>

using namespace glm;
using std::string;

using namespace engine;

unsigned int binding_positions = 0;
unsigned int binding_normals = 1;

int attrib_positions = 0;
int attrib_normals = 1;
int attrib_tex_coords = 2;

void gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                         GLsizei length, GLchar const *message,
                         void const *user_param)
{
    const string source_string{[source]()
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
                                       return "Other";
                                   }
                               }()};

    const string type_string{[type]()
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
                                     return "Other";
                                 }
                             }()};

    const string severity_string{[severity]()
                                 {
                                     switch (severity)
                                     {
                                     case GL_DEBUG_SEVERITY_NOTIFICATION:
                                         return "Notification";
                                     case GL_DEBUG_SEVERITY_LOW:
                                         return "Low";
                                     case GL_DEBUG_SEVERITY_MEDIUM:
                                         return "Medium";
                                     case GL_DEBUG_SEVERITY_HIGH:
                                         return "High";
                                     }
                                 }()};

    std::cout << "(GL message) Type: " << type_string
              << ", Severity: " << severity_string
              << ", Source: " << source_string << ", Message: " << message
              << std::endl;
}

Renderer::Renderer(Shader shadow, Shader skybox, unsigned int skybox_texture)
    : shadow_shader{shadow}, skybox_shader{skybox}, texture_skybox{
                                                        skybox_texture}
{
    // Enable error callback.
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_message_callback, nullptr);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glClearColor(0.f, 0.f, 0.f, 1.0f);

    light.position = vec3{4.f, 3.f, 3.f};

    // Entity stuff.
    {
        glCreateVertexArrays(1, &vao_entities);

        glEnableVertexArrayAttrib(vao_entities, attrib_positions);
        glEnableVertexArrayAttrib(vao_entities, attrib_normals);

        glVertexArrayAttribFormat(vao_entities, attrib_positions, 3, GL_FLOAT,
                                  false, 0);
        glVertexArrayAttribBinding(vao_entities, attrib_positions,
                                   binding_positions);

        glVertexArrayAttribFormat(vao_entities, attrib_normals, 3, GL_FLOAT,
                                  false, 0);
        glVertexArrayAttribBinding(vao_entities, attrib_normals,
                                   binding_normals);
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

    // Depth texture rendered from light perspective.
    glCreateTextures(GL_TEXTURE_2D, 1, &texture_shadow);

    glTextureParameteri(texture_shadow, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(texture_shadow, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(texture_shadow, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(texture_shadow, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Areas beyond the coverage of the shadow map are in light.
    vec4 border_color{1.f};
    glTextureParameterfv(texture_shadow, GL_TEXTURE_BORDER_COLOR,
                         value_ptr(border_color));

    // TODO: internal format.
    glTextureStorage2D(texture_shadow, 1, GL_DEPTH_COMPONENT24,
                       shadow_map_size.x, shadow_map_size.y);
    glTextureSubImage2D(texture_shadow, 0, 0, 0, shadow_map_size.x,
                        shadow_map_size.y, GL_DEPTH_COMPONENT, GL_FLOAT,
                        nullptr);

    glCreateFramebuffers(1, &fbo_shadow);
    glNamedFramebufferTexture(fbo_shadow, GL_DEPTH_ATTACHMENT, texture_shadow,
                              0);
    // We only care about the depth test.
    glNamedFramebufferDrawBuffer(fbo_shadow, GL_NONE);
    glNamedFramebufferReadBuffer(fbo_shadow, GL_NONE);
}

Renderer::~Renderer()
{
    for (auto &r : queue)
        glDeleteBuffers(1, &r.buffer);

    glDeleteVertexArrays(1, &vao_entities);
}

void Renderer::register_entity(const Entity &e)
{
    unsigned int buffer;
    glCreateBuffers(1, &buffer);

    int primitive_count = e.mesh->indices.size();

    int size_indices = e.mesh->indices.size() * sizeof(uint32_t);
    int size_positions = e.mesh->positions.size() * sizeof(vec3);
    int size_normals = e.mesh->normals.size() * sizeof(vec3);
    int size_tex_coords = e.mesh->tex_coords.size() * sizeof(vec2);

    // TODO: Investigate if interleaved storage is more efficient than
    // sequential storage for vertex data.
    glNamedBufferStorage(buffer, size_indices + size_positions + size_normals,
                         nullptr, GL_DYNAMIC_STORAGE_BIT);

    int offset = 0;
    glNamedBufferSubData(buffer, offset, size_indices, e.mesh->indices.data());
    offset += size_indices;
    glNamedBufferSubData(buffer, offset, size_positions,
                         e.mesh->positions.data());
    offset += size_positions;
    glNamedBufferSubData(buffer, offset, size_normals, e.mesh->normals.data());
    offset += size_normals;

    mat4 model = e.transform.get_model();

    queue.push_back(RenderData{
        e.flags,
        buffer,
        model,
        primitive_count,
        size_indices,
        size_indices + size_positions,
        *e.shader,
    });
}

void Renderer::render_data(unsigned int vao, RenderData data)
{
    ZoneScoped;

    glVertexArrayElementBuffer(vao, data.buffer);
    glVertexArrayVertexBuffer(vao, binding_positions, data.buffer,
                              data.positions_offset, sizeof(vec3));
    glVertexArrayVertexBuffer(vao, binding_normals, data.buffer,
                              data.normals_offset, sizeof(vec3));

    glDrawElements(GL_TRIANGLES, data.primitive_count, GL_UNSIGNED_INT,
                   nullptr);
}

void Renderer::render()
{

    const mat4 proj = perspective(radians(90.f),
                                  static_cast<float>(viewport_size.x) /
                                      static_cast<float>(viewport_size.y),
                                  0.1f, 100.f);

    const mat4 view = camera.get_view();

    const mat4 light_transform =
        ortho(-10.f, 10.f, -10.f, 10.f, 1.f, 17.5f) *
        lookAt(light.position, vec3{0.f}, vec3{0.f, 1.f, 0.f});

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
                render_data(vao_entities, r);
            }
        }

        glCullFace(GL_BACK);
    }

    // Render entities.
    {
        TracyGpuZone("Entities");

        glViewport(0, 0, viewport_size.x, viewport_size.y);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(vao_entities);

        uint cur_shader_id;

        for (auto &r : queue)
        {
            if (auto shader_id = r.shader.get_id(); cur_shader_id != shader_id)
            {
                glUseProgram(shader_id);
                cur_shader_id = shader_id;
            }

            const auto model_view = view * r.model;
            const auto mvp = proj * model_view;

            glBindTextureUnit(0, texture_shadow);

            r.shader.set("u_light_pos", vec3(view * vec4(light.position, 1)));
            r.shader.set("u_light_transform", light_transform);

            r.shader.set("u_model", r.model);
            r.shader.set("u_model_view", model_view);
            r.shader.set("u_mvp", mvp);
            r.shader.set("u_normal_mat", inverseTranspose(mat3{model_view}));

            render_data(vao_entities, r);
        }
    }

    // Render skybox.
    {
        TracyGpuZone("Skybox");

        glUseProgram(skybox_shader.get_id());
        glBindTextureUnit(0, texture_skybox);

        skybox_shader.set("u_projection", proj);
        skybox_shader.set("u_view", mat4(mat3(view)));

        glBindVertexArray(vao_skybox);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}