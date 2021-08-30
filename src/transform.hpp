#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine
{

class Transform
{
  public:
    glm::vec3 position;
    glm::vec3 scale;
    glm::quat rotation;

    Transform(glm::vec3 position = glm::vec3{0}, glm::vec3 scale = glm::vec3{1},
              glm::quat rotation = glm::quat{});

    explicit Transform(glm::mat4 matrix);

    void set_euler_angles(float x, float y, float z);
    void set_euler_angles(const glm::vec3 &angles);

    glm::mat4 get_model() const;
};

} // namespace engine