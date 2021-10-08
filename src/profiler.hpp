#pragma once

#include <array>
#include <cstddef>

#include <glad/glad.h>

#include <Tracy.hpp>
#include <TracyOpenGL.hpp>

namespace engine
{

constexpr size_t query_count = 10;

class GpuZone
{
  public:
    GpuZone(size_t idx);
    ~GpuZone();
};

void profiler_init();

void zone_start(size_t idx);

void zone_stop();

void profiler_collect();

uint64_t profiler_zone_time(size_t idx);

} // namespace engine