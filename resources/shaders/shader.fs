#version 460 core

out vec4 frag_color;

uniform vec3 u_light_pos;
uniform mat4 u_model_view;

in Varying
{
	// View space vectors.
	vec3 position;
	vec3 normal;
	vec2 tex_coords;
} fs_in;

void main()
{
	vec3 l = normalize(u_light_pos - fs_in.position);
	vec3 v = normalize(-fs_in.position);
	vec3 n = normalize(fs_in.normal);
	vec3 h = normalize(l + v);

	float i = max(dot(n, h), 0);
	float s = pow(i, 2);

	vec3 ambient = vec3(0.3);

	vec3 light_col = vec3(1);

	vec3 col = ambient + i * light_col + s;

	frag_color = vec4(col, 1);
}