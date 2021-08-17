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

	float ambient = 0.3;
	float diffuse = max(dot(n, l), 0);
	float spec = pow(max(dot(n, h), 0), 16);

	vec3 ambient_color = vec3(1, 1, 0);
	vec3 diffuse_color = vec3(1, 1, 0);
	vec3 light_color = vec3(1);

	vec3 col = ambient * ambient_color + diffuse * diffuse_color + spec * light_color;

	frag_color = vec4(col, 1);
}