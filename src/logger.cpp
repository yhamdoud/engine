#include "logger.hpp"

using namespace engine;
using namespace std;

Logger engine::logger{};

ostream &operator<<(ostream &os, LogType type)
{
    os << entry_type_strings
            [static_cast<typename underlying_type<LogType>::type>(type)];
    return os;
}
