#version 460 core

uniform uint u_id;

layout(location = 3) out uint id;

void main()
{
	id = u_id;
}