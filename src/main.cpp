#define GLFW_INCLUDE_NONE

#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>

#include "constants.hpp"
#include "entity.hpp"
#include "model.hpp"
#include "renderer.hpp"
#include "shader.hpp"
#include "transform.hpp"

using std::array;
using std::optional;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::variant;
using std::vector;
using std::filesystem::path;

using namespace glm;

using namespace engine;

unsigned int window_width = 1280;
unsigned int window_height = 720;

const path resources_path{"../resources"};
const path textures_path = resources_path / "textures";
const path shaders_path = resources_path / "shaders";
const path models_path = resources_path / "models";

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << std::endl;
    abort();
}

static void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    window_height = width;
    window_height = height;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods)
{
    auto *renderer =
        reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    else if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        renderer->camera.reset();
    }
}

static void scroll_callback(GLFWwindow *window, double x_offset,
                            double y_offset)
{
    auto *renderer =
        reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));

    renderer->camera.zoom(y_offset);
}

vec2 get_cursor_position(GLFWwindow *window)
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return {x, y};
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

    auto phong_shader = create_program(ProgramSources{
        .vert = shaders_path / "shader.vs",
        .frag = shaders_path / "shader.fs",
    });

    auto skybox_shader = create_program(ProgramSources{
        .vert = shaders_path / "skybox.vs",
        .frag = shaders_path / "skybox.fs",
    });

    auto shadow_shader = create_program(ProgramSources{
        .vert = shaders_path / "shadow_map.vs",
    });

    const path skybox_path = textures_path / "skybox-1";
    auto skybox_texture =
        upload_cube_map({skybox_path / "right.jpg", skybox_path / "left.jpg",
                         skybox_path / "top.jpg", skybox_path / "bottom.jpg",
                         skybox_path / "front.jpg", skybox_path / "back.jpg"});

    if (skybox_texture == invalid_texture_id)
        return EXIT_FAILURE;

    Renderer renderer{phong_shader, shadow_shader, skybox_shader,
                      skybox_texture};
    glfwSetWindowUserPointer(window, &renderer);

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

    renderer.register_entity(Entity{
        .flags = Entity::Flags::casts_shadow,
        .transform = Transform{vec3{0.f, 0.5f, 0.f}, vec3{0.5f}},
        .mesh = std::make_shared<Mesh>(
            Mesh::from_gtlf(models_path / "cube.gltf")[0]),
    });

    renderer.register_entity(duck1);
    renderer.register_entity(duck2);
    renderer.register_entity(plane);

    double last_time = glfwGetTime();
    vec2 cursor_pos = get_cursor_position(window);

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
                renderer.camera.pan(delta);
            else
                renderer.camera.rotate(delta);
        }

        cursor_pos = new_cursor_pos;

        renderer.viewport_size = vec2{window_width, window_height};
        renderer.render();

        glfwSwapBuffers(window);
        glfwPollEvents();

        TracyGpuCollect;

        FrameMark
    }

    glfwTerminate();

    return EXIT_SUCCESS;
}
