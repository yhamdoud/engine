#include <bitset>
#include <numeric>

#include "logger.hpp"
#include "profiler.hpp"

using namespace engine;
using namespace std;

const int sample_count = 15;

std::array<std::array<uint, query_count>, 2> queries{};
std::bitset<query_count> active_queries;
std::array<std::array<uint64_t, sample_count>, query_count> times;

int active = 0;

uint *front() { return queries[active].data(); }

uint *back() { return queries[active ^ 1].data(); }

void swap() { active ^= 1; }

void engine::profiler_init()
{
    glGenQueries(2 * query_count, reinterpret_cast<uint *>(queries.data()));
}

GpuZone::GpuZone(size_t idx)
{
    glBeginQuery(GL_TIME_ELAPSED, back()[idx]);
    active_queries[idx] = true;
}

GpuZone::~GpuZone() { glEndQuery(GL_TIME_ELAPSED); }

uint counter = 0;

void engine::profiler_collect()
{
    counter = (counter + 1) % sample_count;
    uint64_t timer;

    for (int i; i < query_count; i++)
    {
        if (active_queries[i])
        {
            glGetQueryObjectui64v(front()[i], GL_QUERY_RESULT, &timer);
            times[i][counter] = timer;
        }
        else
        {
            times[i][counter] = 0.f;
        }
    }

    active_queries.reset();

    swap();
}

uint64_t engine::profiler_zone_time(size_t idx)
{
    return accumulate(times[idx].begin(), times[idx].end(), 0ul) /
           times[idx].size();
}