#pragma once

#include <glm/glm.hpp>

#include "constants.hpp"

namespace engine
{

float halton(uint32_t index, uint32_t base);

} // namespace engine