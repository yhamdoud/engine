#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include "renderer/light.hpp"

using namespace engine;
using namespace glm;

float Light::radius_squared(float eps) const
{
    const float luminance = intensity * compMax(color);
    return luminance / eps;
}