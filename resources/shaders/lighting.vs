// Draw a fullscreen triangle.

#version 460 core

uniform mat4 u_proj_inv;

out vec2 tex_coords;
out vec3 view_ray;

void main() {
    vec2 vertices[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
    // Place the triangle vertices at the far plane of the camera for later
    // position reconstruction.
    gl_Position = vec4(vertices[gl_VertexID], 1, 1);
    tex_coords = 0.5 * gl_Position.xy + vec2(0.5);
    vec4 t = u_proj_inv * gl_Position;
    view_ray = vec3(t) / t.w;
}
