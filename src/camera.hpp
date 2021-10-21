// Arcball-style camera

#pragma once

#include <glm/glm.hpp>

class Camera
{
    glm::vec3 target;
    glm::vec3 up;
    glm::vec3 offset;

    float look_sensitivity = 0.01f;
    float pan_sensitivity = 0.01f;

  public:
    glm::vec3 position;

    Camera(glm::vec3 position, glm::vec3 target,
           glm::vec3 up = glm::vec3(0, 1, 0));

    void rotate(glm::vec2 delta);
    void pan(glm::vec2 delta);
    void zoom(float direction);
    void reset();

    glm::mat4 get_view() const;
};