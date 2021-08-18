#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include "transform.hpp"

using namespace glm;

using namespace engine;

Transform::Transform(vec3 p, vec3 s, quat r)
    : position{p}, scale{s}, rotation{r}
{
}

void Transform::set_euler_angles(float x, float y, float z)
{
    set_euler_angles(vec3{x, y, z});
}

void Transform::set_euler_angles(const vec3 &angles)
{
    rotation = quat{angles};
}

mat4 Transform::get_model() const
{
    return glm::scale(mat4_cast(rotation) * translate(mat4{1}, position),
                      scale);
}