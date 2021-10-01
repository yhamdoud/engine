#pragma once

#include <array>

#include <glm/glm.hpp>

#include "constants.hpp"

namespace engine
{

class ProbeBuffer
{
  public:
    static constexpr size_t count = 7;

    ProbeBuffer(glm::ivec3 size);
    ~ProbeBuffer();

    ProbeBuffer(const ProbeBuffer &) = delete;
    ProbeBuffer &operator=(const ProbeBuffer &) = delete;
    ProbeBuffer(ProbeBuffer &&) = delete;
    ProbeBuffer &operator=(ProbeBuffer &&) = delete;

    glm::ivec3 get_size() const;
    void swap();
    void clear();
    void resize(glm::ivec3 size);
    uint *front();
    uint *back();

  private:
    int active_buf = 0;
    glm::ivec3 size;
    std::array<std::array<uint, count>, 2> data{};

    void allocate(glm::ivec3 size);
};

} // namespace engine