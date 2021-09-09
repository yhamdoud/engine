#version 460

in vec2 tex_coords;

uniform sampler2D u_hdr_screen;
uniform bool u_do_tonemap;
uniform bool u_do_gamma_correct;
uniform float u_exposure;
uniform float u_gamma;

out vec4 frag_color;

vec3 reinhard(vec3 hdr, float exposure)
{
	return vec3(1.) - exp(-hdr * exposure);
}

vec3 gamma_correct(vec3 linear, float gamma)
{
	return pow(linear, vec3(1. / gamma));
}

void main()
{
	vec3 hdr = texture(u_hdr_screen, tex_coords).rgb;

	if (u_do_tonemap)
		// Now LDR...
		hdr = reinhard(hdr, u_exposure);

	if (u_do_gamma_correct)
		hdr = gamma_correct(hdr, u_gamma);

	frag_color = vec4(hdr, 1.);
}