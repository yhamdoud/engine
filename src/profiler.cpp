#include <bitset>
#include <cstdint>
#include <numeric>

#include "logger.hpp"
#include "profiler.hpp"
#include "constants.hpp"

using namespace engine;
using namespace std;

const int sample_count = 15;

struct Query
{
    uint begin;
    uint end;
};

std::array<std::array<Query, query_count>, 2> queries{};
std::bitset<query_count> active_queries;
std::array<std::array<uint64_t, sample_count>, query_count> times;

int active = 0;
uint counter = 0;

Query *front() { return queries[active].data(); }

Query *back() { return queries[active ^ 1].data(); }

void swap() { active ^= 1; }

void engine::profiler_init()
{
    glGenQueries(4 * query_count, reinterpret_cast<uint *>(queries.data()));
}

GpuZone::GpuZone(size_t idx) : idx(idx)
{
    glQueryCounter(back()[idx].begin, GL_TIMESTAMP);
    active_queries[idx] = true;
}

GpuZone::~GpuZone() { glQueryCounter(back()[idx].end, GL_TIMESTAMP); };

void engine::profiler_collect()
{
    uint64_t begin, end;

    for (int i = 0; i < query_count; i++)
    {
        if (active_queries[i])
        {
            int available = 0;
            glGetQueryObjectiv(front()[i].end, GL_QUERY_RESULT_AVAILABLE,
                               &available);
            if (available)
            {
                glGetQueryObjectui64v(front()[i].begin, GL_QUERY_RESULT,
                                      &begin);
                glGetQueryObjectui64v(front()[i].end, GL_QUERY_RESULT, &end);

                times[i][counter] = end - begin;
            }
            else
            {
                logger.warn("Timer query result unavailable.");
            }
        }
        else
        {
            times[i][counter] = 0.f;
        }
    }

    counter = (counter + 1) % sample_count;

    active_queries.reset();

    swap();
}

uint64_t engine::profiler_zone_time(size_t idx)
{
    return accumulate(times[idx].begin(), times[idx].end(), 0ul) /
           times[idx].size();
}