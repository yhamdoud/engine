#define GLM_SWIZZLE

#include <glm/ext.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include "camera.hpp"

using namespace glm;

Camera::Camera(vec3 position, vec3 target, vec3 up)
    : position{position}, target{target}, up{normalize(up)}, offset{vec3{0}}
{
}

void Camera::pan(glm::vec2 delta)
{
    delta = delta * pan_sensitivity;
    // Pan slower when zoomed in.
    delta *= length(position);

    vec3 forward = target - position;
    vec3 right = normalize(cross(forward, up));
    vec3 pan_up = normalize(cross(right, forward));

    offset += delta.x * right - delta.y * pan_up;
}

void Camera::rotate(glm::vec2 delta)
{
    delta = delta * look_sensitivity;
    delta.y /= 2.f;

    auto right = normalize(cross(target - position, up));

    position =
        vec3{glm::rotate(glm::rotate(mat4{1.f}, delta.x, up), delta.y, right) *
             vec4{position, 1.f}};
}

void Camera::zoom(float direction)
{
    if (direction < 0)
        position *= 1.05;
    else
        position *= 0.95;
}

glm::mat4 Camera::get_view() const
{
    return lookAt(position + offset, target + offset, up);
}