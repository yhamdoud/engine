#include <cstdint>
#include <glad/glad.h>

#include "buffer.hpp"
#include "logger.hpp"

using namespace std;
using namespace engine;

Buffer::Buffer(uint32_t capacity, uint32_t alignment) : capacity(capacity)
{
    glCreateBuffers(1, &id);
    glNamedBufferStorage(id, capacity, nullptr, GL_DYNAMIC_STORAGE_BIT);
};

uint Buffer::get_id() { return id; }

// TODO: Use a better allocation strategy. Allow for deallocation, and fill
// missing gaps.
uint32_t Buffer::allocate(const void *data, uint32_t alloc_size)
{
    while (size + alloc_size >= capacity)
    {
        capacity *= 2;
        uint new_id;
        glCreateBuffers(1, &new_id);
        glNamedBufferStorage(new_id, capacity, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glCopyNamedBufferSubData(id, new_id, 0, 0, size);
        id = new_id;
    }

    uint32_t offset = size;
    glNamedBufferSubData(id, offset, alloc_size, data);

    size += alloc_size;

    return offset;
};