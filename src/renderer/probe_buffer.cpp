#include <glad/glad.h>

#include "renderer/probe_buffer.hpp"

using namespace engine;
using namespace glm;

ProbeBuffer::ProbeBuffer(ivec3 size) : size(size) { allocate(size); }

ProbeBuffer::~ProbeBuffer() { clear(); }

void ProbeBuffer::resize(ivec3 size)
{
    this->size = size;
    clear();
    allocate(size);
}

void ProbeBuffer::allocate(ivec3 size)
{
    constexpr float clear_color[4] = {0.f, 0.f, 0.f, 0.f};

    glCreateTextures(GL_TEXTURE_3D, 2 * count, data[0].data());

    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < count; j++)
        {
            glTextureStorage3D(data[i][j], 1, GL_RGBA16F, size.x, size.y,
                               size.z);

            glTextureParameteri(data[i][j], GL_TEXTURE_WRAP_S,
                                GL_CLAMP_TO_EDGE);
            glTextureParameteri(data[i][j], GL_TEXTURE_WRAP_T,
                                GL_CLAMP_TO_EDGE);
            glTextureParameteri(data[i][j], GL_TEXTURE_WRAP_R,
                                GL_CLAMP_TO_EDGE);

            // Important for trilinear interpolation.
            glTextureParameteri(data[i][j], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(data[i][j], GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_LINEAR);

            glClearTexImage(data[i][j], 0, GL_RGBA, GL_FLOAT, &clear_color);
        }
    }
}

void ProbeBuffer::clear() { glDeleteTextures(2 * count, data[0].data()); }

ivec3 ProbeBuffer::get_size() const { return size; }

void ProbeBuffer::swap() { active_buf ^= 1; }

uint *ProbeBuffer::front() { return data[active_buf].data(); }

uint *ProbeBuffer::back() { return data[active_buf ^ 1].data(); }