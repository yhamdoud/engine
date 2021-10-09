#pragma once

#include "constants.hpp"

namespace engine
{

class Buffer
{
    uint id;
    uint32_t capacity;
    uint32_t size;

  public:
    Buffer(uint32_t capacity, uint32_t alignment);

    uint get_id();
    uint32_t allocate(const void *data, uint32_t alloc_size);
};

} // namespace engine