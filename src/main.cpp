#include <cstddef>
#define GLFW_INCLUDE_NONE
#define STB_IMAGE_IMPLEMENTATION

#include <array>
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
    glViewport(0, 0, window_width, window_height);
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
    return create_shader(from_file(path), type);
}

unsigned int create_program(unsigned int vert_shader, unsigned int frag_shader)
{
    unsigned int program = glCreateProgram();
    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
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

unsigned int create_program(const path &vert_shader, const path &frag_shader)
{
    return create_program(create_shader(vert_shader, GL_VERTEX_SHADER),
                          create_shader(frag_shader, GL_FRAGMENT_SHADER));
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

struct Entity
{
    Transform transform;
    shared_ptr<Mesh> mesh;
};

struct RenderData
{
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

    return RenderData{buffer, model, primitive_count, size_indices,
                      size_indices + size_positions};
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

    glNamedBufferStorage(buffer, sizeof(primitives::cube), primitives::cube,
                         GL_DYNAMIC_STORAGE_BIT);
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

    unsigned int vert_shader =
        create_shader(shaders_path / "shader.vs", GL_VERTEX_SHADER);
    unsigned int frag_shader =
        create_shader(shaders_path / "shader.fs", GL_FRAGMENT_SHADER);
    unsigned int program = create_program(vert_shader, frag_shader);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    unsigned int vao;
    glCreateVertexArrays(1, &vao);

    glEnableVertexArrayAttrib(vao, attrib_positions);
    glEnableVertexArrayAttrib(vao, attrib_normals);

    glVertexArrayAttribFormat(vao, attrib_positions, 3, GL_FLOAT, false, 0);
    glVertexArrayAttribBinding(vao, attrib_positions, binding_positions);

    glVertexArrayAttribFormat(vao, attrib_normals, 3, GL_FLOAT, false, 0);
    glVertexArrayAttribBinding(vao, attrib_normals, binding_normals);

    vector<RenderData> render_queue;

    auto mesh =
        std::make_shared<Mesh>(Mesh::from_gtlf(models_path / "duck.glb")[0]);

    Entity duck1{Transform{vec3{2, 0, 1}, vec3{0.01f}}, mesh};
    duck1.transform.set_euler_angles(radians(60.f), radians(20.f),
                                     radians(90.f));

    Entity duck2{Transform{vec3{3, 3, 1}, vec3{0.01f}}, mesh};

    render_queue.push_back(register_entity(duck1));
    render_queue.push_back(register_entity(duck2));

    glClearColor(0.f, 0.f, 0.f, 1.0f);

    Uniform uniform_mvp, uniform_normal_mat, uniform_model_view,
        uniform_light_pos;

    if (auto uniform_map = parse_uniforms(program))
    {
        try
        {
            uniform_mvp = uniform_map->at("u_mvp");
            uniform_normal_mat = uniform_map->at("u_normal_mat");
            uniform_model_view = uniform_map->at("u_model_view");
            uniform_light_pos = uniform_map->at("u_light_pos");
        }
        catch (std::exception e)
        {
            std::cerr << "Missing uniform in shader" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        std::cerr << "No uniforms in shader" << std::endl;
        exit(EXIT_FAILURE);
    }

    double last_time = glfwGetTime();

    mat4 model = glm::scale(mat4{1}, vec3{0.01f});

    vec2 cursor_pos = get_cursor_position(window);

    const vec4 light_pos = vec4{10, 10, 10, 1};

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

    auto skybox_shader =
        create_program(shaders_path / "skybox.vs", shaders_path / "skybox.fs");

    auto skybox_uniforms = *parse_uniforms(skybox_shader);

    auto skybox_vao = init_skybox();

    // Enable profiling.
    TracyGpuContext;

    while (!glfwWindowShouldClose(window))
    {
        TracyGpuZone("Render");

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

        const mat4 persp =
            glm::perspective(glm::radians(90.f),
                             static_cast<float>(window_width) /
                                 static_cast<float>(window_height),
                             0.1f, 100.f);

        const mat4 view = camera.get_view();

        // Render entities.
        {
            glUseProgram(program);
            set_uniform(program, uniform_model_view, view * model);
            set_uniform(program, uniform_light_pos,
                        vec3(view * vec4(light_pos)));

            glBindVertexArray(vao);

            for (auto &r : render_queue)
            {
                const auto model_view = view * r.model;
                const auto mvp = persp * model_view;
                set_uniform(program, uniform_mvp, mvp);
                set_uniform(program, uniform_normal_mat,
                            glm::inverseTranspose(mat3{model_view}));

                render(vao, r);
            }
        }

        // Render skybox.
        {
            glUseProgram(skybox_shader);
            glBindTextureUnit(0, *skybox_texture);

            set_uniform(skybox_shader, skybox_uniforms["u_projection"], persp);
            set_uniform(skybox_shader, skybox_uniforms["u_view"],
                        mat4(mat3(view)));

            render_skybox(skybox_vao);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();

        TracyGpuCollect;

        FrameMark
    }

    for (auto &r : render_queue)
        glDeleteBuffers(1, &r.buffer);

    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);

    glfwTerminate();

    return EXIT_SUCCESS;
}
