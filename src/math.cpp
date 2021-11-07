#include <cmath>

#include "math.hpp"

// using namespace engine;
using namespace glm;

// https://en.wikipedia.org/wiki/Halton_sequence
float engine::halton(uint32_t index, uint32_t base)
{
    float f = 1.f;
    float r = 0.f;

    while (index > 0)
    {
        auto base_float = static_cast<float>(base);
        auto index_float = static_cast<float>(index);

        f /= base_float;
        r += f * static_cast<float>(index % base);
        index = static_cast<uint32_t>(floorf(index_float / base_float));
    }

    return r;
}