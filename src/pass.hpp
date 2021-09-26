#pragma once

#include "render_context.hpp"

namespace engine
{

template <typename T>
concept Pass = requires(T t, RenderContext r, ViewportContext v)
{
    t.initialize(v);
    t.render(v, r);
};

} // namespace engine