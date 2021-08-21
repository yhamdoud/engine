#define STB_IMAGE_IMPLEMENTATION
#define GLFW_INCLUDE_NONE

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <streambuf>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <stb_image.h>

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>

#include "camera.hpp"
#include "entity.hpp"
#include "model.hpp"
#include "primitives.hpp"
#include "transform.hpp"

using std::array;
using std::optional;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;
using std::variant;
using std::vector;
using std::filesystem::path;

using namespace glm;

using namespace engine;

unsigned int window_width = 1280;
unsigned int window_height = 720;

Camera camera{vec3{0, 0, 4}, vec3{0}};

const path resources_path{"../resources"};
const path textures_path = resources_path / "textures";
const path shaders_path = resources_path / "shaders";
const path models_path = resources_path / "models";

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << std::endl;
    abort();
}

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

static void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    window_height = width;
    window_height = height;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    else if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        camera.reset();
    }
}

static void scroll_callback(GLFWwindow *window, double x_offset,
                            double y_offset)
{
    camera.zoom(y_offset);
}

std::string from_file(const path &path)
{
    std::ifstream file{path};
    return string{std::istreambuf_iterator<char>(file),
                  std::istreambuf_iterator<char>()};
}

unsigned int create_shader(const string &source, GLenum type)
{
    unsigned int shader = glCreateShader(type);

    auto c_str = source.c_str();
    glShaderSource(shader, 1, &c_str, nullptr);

    glCompileShader(shader);

    int is_compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
    if (!is_compiled)
    {
        int length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        auto log = std::make_unique<char[]>(length);
        glGetShaderInfoLog(shader, length, nullptr, log.get());
        std::cerr << "Shader failure: " << log << std::endl;
    }

    return shader;
}

unsigned int create_shader(const path &path, GLenum type)
{
    if (!std::filesystem::exists(path))
    {
        std::cerr << "Shader not found: " << path << std::endl;
        return 0;
    }

    return create_shader(from_file(path), type);
}

struct ProgramSources
{
    path vert{};
    path geom{};
    path frag{};
};

unsigned int create_program(unsigned int vert, unsigned int geom,
                            unsigned int frag)
{
    unsigned int program = glCreateProgram();

    glAttachShader(program, vert);

    if (geom != 0)
        glAttachShader(program, geom);

    if (frag != 0)
        glAttachShader(program, frag);

    glLinkProgram(program);

    int is_linked;
    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (!is_linked)
    {
        int length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        auto log = std::make_unique<char[]>(length);
        glGetProgramInfoLog(program, length, nullptr, log.get());
        std::cerr << "Shader program failure: " << log << std::endl;
    }

    return program;
}

unsigned int create_program(const ProgramSources &srcs)
{
    auto vert = create_shader(srcs.vert, GL_VERTEX_SHADER);
    auto geom =
        !srcs.geom.empty() ? create_shader(srcs.geom, GL_GEOMETRY_SHADER) : 0;
    auto frag =
        !srcs.frag.empty() ? create_shader(srcs.frag, GL_FRAGMENT_SHADER) : 0;

    return create_program(vert, geom, frag);

    glDeleteShader(vert);
    glDeleteShader(geom);
    glDeleteShader(frag);
}

struct Uniform
{
    int location;
    int count;
};

optional<unordered_map<string, Uniform>> parse_uniforms(unsigned int program)
{
    int count;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);

    if (count == 0)
        return std::nullopt;

    int max_length;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_length);

    auto name = std::make_unique<char[]>(max_length);

    unordered_map<string, Uniform> name_uniform_map(count);

    int length, size;
    GLenum type;

    for (int i = 0; i < count; i++)
    {
        glGetActiveUniform(program, i, max_length, &length, &size, &type,
                           &name[0]);

        Uniform uniform{glGetUniformLocation(program, name.get()), size};
        name_uniform_map.emplace(
            std::make_pair(string(name.get(), length), uniform));
    }

    return std::make_optional(name_uniform_map);
}

void set_uniform(unsigned int program, Uniform uniform, const mat4 &value)
{
    glProgramUniformMatrix4fv(program, uniform.location, uniform.count, false,
                              glm::value_ptr(value));
}

void set_uniform(unsigned int program, Uniform uniform, const mat3 &value)
{
    glProgramUniformMatrix3fv(program, uniform.location, uniform.count, false,
                              glm::value_ptr(value));
}

void set_uniform(unsigned int program, Uniform uniform, const vec3 &value)
{
    glProgramUniform3fv(program, uniform.location, uniform.count,
                        glm::value_ptr(value));
}

vec2 get_cursor_position(GLFWwindow *window)
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return vec2(x, y);
}

struct RenderData
{
    Entity::Flags flags;
    GLuint buffer;
    mat4 model;
    int primitive_count;
    int positions_offset;
    int normals_offset;
};

unsigned int binding_positions = 0;
unsigned int binding_normals = 1;

int attrib_positions = 0;
int attrib_normals = 1;
int attrib_tex_coords = 2;

RenderData register_entity(const Entity &e)
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

    return RenderData{
        e.flags,         buffer,       model,
        primitive_count, size_indices, size_indices + size_positions,
    };
}

optional<GLuint> upload_cube_map(const array<path, 6> &paths)
{
    unsigned int id;

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &id);

    GLenum format = GL_RGB;

    for (size_t face_idx = 0; face_idx < 6; face_idx++)
    {
        const auto &path = paths[face_idx];

        int width, height, channel_count;

        uint8_t *data =
            stbi_load(path.c_str(), &width, &height, &channel_count, 0);

        if (face_idx == 0)
            glTextureStorage2D(id, 1, GL_RGBA8, width, height);

        if (data)
        {
            glad_glTextureSubImage3D(id, 0, 0, 0, face_idx, width, height, 1,
                                     format, GL_UNSIGNED_BYTE, data);

            std::free(data);
        }
        else
        {
            std::cerr << "Failed to load texture: " << path << std::endl;
            return std::nullopt;
        }
    }

    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return std::make_optional(id);
}

// Return skybox VAO.
unsigned int init_skybox()
{
    unsigned int buffer;
    glCreateBuffers(1, &buffer);

    unsigned int vao;
    glCreateVertexArrays(1, &vao);

    unsigned int binding = 0, attribute = 0;

    glVertexArrayAttribFormat(vao, attribute, 3, GL_FLOAT, false, 0);
    glVertexArrayAttribBinding(vao, attribute, binding);
    glEnableVertexArrayAttrib(vao, attribute);

    glNamedBufferStorage(buffer, sizeof(primitives::cube_verts),
                         primitives::cube_verts.data(), GL_NONE);
    glVertexArrayVertexBuffer(vao, binding_positions, buffer, 0, sizeof(vec3));

    return vao;
}

void render_skybox(unsigned int vao)
{
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void render(unsigned int vao, RenderData data)
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

enum class Error
{
    glfw_initialization,
    window_creation,
};

variant<GLFWwindow *, Error> init_glfw()
{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
        return Error::glfw_initialization;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(window_width, window_height,
                                          "triangle", nullptr, nullptr);
    if (window == nullptr)
        return Error::window_creation;

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwMakeContextCurrent(window);

    return window;
}

int main()
{

    auto window_variant = init_glfw();

    if (auto error = std::get_if<Error>(&window_variant))
    {
        switch (*error)
        {
        case Error::glfw_initialization:
            std::cout << "GLFW initialization failed." << std::endl;
            break;
        case Error::window_creation:
            std::cout << "Window creation failed." << std::endl;
            break;
        }

        return EXIT_FAILURE;
    }

    auto window = std::get<GLFWwindow *>(window_variant);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "glad error" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Enable error callback.
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_message_callback, nullptr);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    unsigned int phong_shader = create_program(ProgramSources{
        .vert = shaders_path / "shader.vs",
        .frag = shaders_path / "shader.fs",
    });

    auto phong_uniforms = *parse_uniforms(phong_shader);

    unsigned int vao_entities;
    glCreateVertexArrays(1, &vao_entities);

    glEnableVertexArrayAttrib(vao_entities, attrib_positions);
    glEnableVertexArrayAttrib(vao_entities, attrib_normals);

    glVertexArrayAttribFormat(vao_entities, attrib_positions, 3, GL_FLOAT,
                              false, 0);
    glVertexArrayAttribBinding(vao_entities, attrib_positions,
                               binding_positions);

    glVertexArrayAttribFormat(vao_entities, attrib_normals, 3, GL_FLOAT, false,
                              0);
    glVertexArrayAttribBinding(vao_entities, attrib_normals, binding_normals);

    vector<RenderData> render_queue;

    auto mesh =
        std::make_shared<Mesh>(Mesh::from_gtlf(models_path / "duck.glb")[0]);

    Entity duck1{.flags = Entity::Flags::casts_shadow,
                 .transform = Transform{vec3{2, 0, 1}, vec3{0.01f}},
                 .mesh = mesh};

    auto duck2 = duck1;
    duck1.transform.set_euler_angles(radians(60.f), radians(20.f),
                                     radians(90.f));
    duck2.transform.position = vec3{3, 3, 1};

    auto plane = Entity{.transform = Transform{},
                        .mesh = std::make_shared<Mesh>(

                            Mesh::from_gtlf(models_path / "plane.gltf")[0])};

    plane.transform.scale = vec3{20.f};
    plane.flags = Entity::Flags::casts_shadow;

    render_queue.push_back(register_entity(Entity{
        .flags = Entity::Flags::casts_shadow,
        .transform = Transform{vec3{0.f, 0.5f, 0.f}, vec3{0.5f}},
        .mesh = std::make_shared<Mesh>(
            Mesh::from_gtlf(models_path / "cube.gltf")[0]),
    }));

    render_queue.push_back(register_entity(duck1));
    render_queue.push_back(register_entity(duck2));
    render_queue.push_back(register_entity(plane));

    glClearColor(0.f, 0.f, 0.f, 1.0f);

    double last_time = glfwGetTime();

    vec2 cursor_pos = get_cursor_position(window);

    const vec4 light_pos = vec4{3, 5, -3, 1};
    Entity light{.transform = Transform{},
                 .mesh = std::make_shared<Mesh>(
                     Mesh::from_gtlf(models_path / "sphere.glb")[0])};
    light.transform.position = light_pos;

    render_queue.push_back(register_entity(light));

    const path skybox_path = textures_path / "skybox-1";

    auto skybox_texture = upload_cube_map({
        skybox_path / "right.jpg",
        skybox_path / "left.jpg",
        skybox_path / "top.jpg",
        skybox_path / "bottom.jpg",
        skybox_path / "front.jpg",
        skybox_path / "back.jpg",
    });

    if (!skybox_texture.has_value())
        return EXIT_FAILURE;

    auto skybox_shader = create_program(ProgramSources{
        .vert = shaders_path / "skybox.vs",
        .frag = shaders_path / "skybox.fs",
    });

    auto skybox_uniforms = *parse_uniforms(skybox_shader);

    auto vao_skybox = init_skybox();

    // Shadow mapping setup.

    // Depth texture rendered from light perspective.
    unsigned int shadow_texture;
    const unsigned int shadow_texture_width = 1024,
                       shadow_texture_height = 1024;
    glCreateTextures(GL_TEXTURE_2D, 1, &shadow_texture);

    glTextureParameteri(shadow_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadow_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadow_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(shadow_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Areas beyond the coverage of the shadow map are in light.
    vec4 border_color{1.f, 1.f, 1.f, 1.f};
    glTextureParameterfv(shadow_texture, GL_TEXTURE_BORDER_COLOR,
                         value_ptr(border_color));

    // TODO: internal format.
    glTextureStorage2D(shadow_texture, 1, GL_DEPTH_COMPONENT24,
                       shadow_texture_width, shadow_texture_height);
    glTextureSubImage2D(shadow_texture, 0, 0, 0, shadow_texture_width,
                        shadow_texture_height, GL_DEPTH_COMPONENT, GL_FLOAT,
                        nullptr);

    unsigned int shadow_fbo;
    glCreateFramebuffers(1, &shadow_fbo);
    glNamedFramebufferTexture(shadow_fbo, GL_DEPTH_ATTACHMENT, shadow_texture,
                              0);
    // We only care about the depth test.
    glNamedFramebufferDrawBuffer(shadow_fbo, GL_NONE);
    glNamedFramebufferReadBuffer(shadow_fbo, GL_NONE);

    auto shadow_shader = create_program(ProgramSources{
        .vert = shaders_path / "shadow_map.vs",
    });

    auto shadow_uniforms = *parse_uniforms(shadow_shader);

    // Enable profiling.
    TracyGpuContext;

    while (!glfwWindowShouldClose(window))
    {
        TracyGpuZone("Render");

        // Timestep.
        float time = glfwGetTime();
        float delta_time = time - last_time;
        last_time = time;

        vec2 new_cursor_pos = get_cursor_position(window);

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
        {
            auto delta = cursor_pos - new_cursor_pos;

            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                camera.pan(delta);
            else
                camera.rotate(delta);
        }

        cursor_pos = new_cursor_pos;

        const mat4 proj =
            glm::perspective(glm::radians(90.f),
                             static_cast<float>(window_width) /
                                 static_cast<float>(window_height),
                             0.1f, 100.f);

        const mat4 view = camera.get_view();

        const mat4 light_transform =
            ortho(-10.f, 10.f, -10.f, 10.f, 1.f, 17.5f) *
            lookAt(vec3{light_pos}, vec3{0.f}, vec3{0.f, 1.f, 0.f});

        // Shadow mapping pass.
        {
            TracyGpuZone("Shadow mapping");

            glCullFace(GL_FRONT);

            glViewport(0, 0, shadow_texture_width, shadow_texture_height);
            glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
            glClear(GL_DEPTH_BUFFER_BIT);

            // Directional light.

            glUseProgram(shadow_shader);
            set_uniform(shadow_shader, shadow_uniforms.at("u_light_transform"),
                        light_transform);

            glBindVertexArray(vao_entities);

            for (auto &r : render_queue)
            {
                if (r.flags & Entity::casts_shadow)
                {
                    set_uniform(shadow_shader, shadow_uniforms["u_model"],
                                r.model);
                    render(vao_entities, r);
                }
            }

            glCullFace(GL_BACK);
        }

        // Render entities.
        {
            TracyGpuZone("Entities");

            glViewport(0, 0, window_width, window_height);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(phong_shader);

            set_uniform(phong_shader, phong_uniforms["u_light_pos"],
                        vec3(view * vec4(light_pos)));

            set_uniform(phong_shader, phong_uniforms.at("u_light_transform"),
                        light_transform);

            glBindVertexArray(vao_entities);

            for (auto &r : render_queue)
            {
                const auto model_view = view * r.model;
                const auto mvp = proj * model_view;

                glBindTextureUnit(0, shadow_texture);

                set_uniform(phong_shader, phong_uniforms["u_model"], r.model);
                set_uniform(phong_shader, phong_uniforms["u_model_view"],
                            model_view);
                set_uniform(phong_shader, phong_uniforms["u_mvp"], mvp);
                set_uniform(phong_shader, phong_uniforms["u_normal_mat"],
                            glm::inverseTranspose(mat3{model_view}));

                render(vao_entities, r);
            }
        }

        // Render skybox.
        {
            TracyGpuZone("Skybox");

            glUseProgram(skybox_shader);
            glBindTextureUnit(0, *skybox_texture);

            set_uniform(skybox_shader, skybox_uniforms["u_projection"], proj);
            set_uniform(skybox_shader, skybox_uniforms["u_view"],
                        mat4(mat3(view)));

            render_skybox(vao_skybox);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();

        TracyGpuCollect;

        FrameMark
    }

    for (auto &r : render_queue)
        glDeleteBuffers(1, &r.buffer);

    glDeleteVertexArrays(1, &vao_entities);
    glDeleteProgram(phong_shader);

    glfwTerminate();

    return EXIT_SUCCESS;
}
