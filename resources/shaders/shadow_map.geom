#version 460 core

#define CASCADE_COUNT 3

layout(triangles, invocations = CASCADE_COUNT) in;
layout(triangle_strip, max_vertices = 3) out;

uniform mat4 u_light_transforms[CASCADE_COUNT];

void main()
{
	for (int i = 0; i < 3; i++)
	{
		gl_Position = u_light_transforms[gl_InvocationID] * gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		EmitVertex();
	}

	EndPrimitive();
}