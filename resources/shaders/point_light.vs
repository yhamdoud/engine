#version 460 core

layout(location = 0) in vec3 a_position;

struct PointLight
{
    vec3 position;
    vec3 color;
    float radius;
    float radius_squared;
};

uniform PointLight u_light;

layout(location = 1) uniform mat4 u_view_proj;

void main()
{
    vec3 pos = u_light.radius * a_position + u_light.position;
    gl_Position = u_view_proj * vec4(pos, 1.);
}