#pragma once

#include "renderer/context.hpp"

namespace engine
{

template <typename T>
concept Pass = requires(T t, RenderContext r, ViewportContext v)
{
    t.initialize(v);
    t.render(v, r);
};

} // namespace engine