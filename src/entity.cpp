
#include <memory>

#include "entity.hpp"

using namespace engine;

constexpr Entity::Flags operator|(Entity::Flags a, Entity::Flags b)
{
    return static_cast<Entity::Flags>(
        static_cast<std::underlying_type<Entity::Flags>::type>(a) |
        static_cast<std::underlying_type<Entity::Flags>::type>(b));
}

constexpr Entity::Flags operator&(Entity::Flags a, Entity::Flags b)
{
    return static_cast<Entity::Flags>(
        static_cast<std::underlying_type<Entity::Flags>::type>(a) &
        static_cast<std::underlying_type<Entity::Flags>::type>(b));
}
