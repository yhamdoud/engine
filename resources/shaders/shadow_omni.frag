#version 460 core

in vec3 v_pos;

uniform vec3 u_light_position;
uniform float u_far;

void main()
{
    float light_distance = distance(v_pos, u_light_position);
	// FIXME: screws with early z
    gl_FragDepth = light_distance / u_far;
}