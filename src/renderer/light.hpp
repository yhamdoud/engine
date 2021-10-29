#pragma once

#include <glm/glm.hpp>

namespace engine
{

struct DirectionalLight
{
    glm::vec3 color{0.f};
    // Intensity is illuminance at perpendicular incidence in lux.
    float intensity = 0.f;
    glm::vec3 direction{0.f};
};

struct Light
{
    glm::vec3 position{0.f};
    glm::vec3 color{0.f};
    float intensity = 0.f;

    float radius_squared(float eps) const;
};

} // namespace engine