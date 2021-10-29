#pragma once

#include <glm/glm.hpp>

namespace engine
{

struct DirectionalLight
{
    glm::vec3 color;
    // Intensity is illuminance at perpendicular incidence in lux.
    float intensity;
    glm::vec3 direction;
};

struct Light
{
    glm::vec3 position;
    glm::vec3 color;
    float intensity;

    float radius_squared(float eps) const;
};

} // namespace engine