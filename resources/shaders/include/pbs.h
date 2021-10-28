#ifndef PBS_H
#define PBS_H

#include "/include/math.h"

// Normal distribution function
// Source: https://google.github.io/filament
float D_GGX(float NoH, float a2)
{
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

// Geometric shadowing
// Source: https://google.github.io/filament
float V_SmithGGXCorrelated(float NoV, float NoL, float a2)
{
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

// Fresnel
// Source: https://google.github.io/filament
vec3 F_Schlick(float u, vec3 f0)
{
    float f = pow(1.0 - u, 5.0);
    return f + f0 * (1.0 - f);
}

// Source: https://google.github.io/filament
float Fd_Lambert() { return 1.0 / PI; }

// Source: https://seblagarde.wordpress.com/2011/08/17/hello-world/
vec3 F_Schlick_roughness(float u, vec3 f0, float roughness)
{
    return f0 +
           (max(vec3(1.0 - roughness), f0) - f0) * pow(saturate(1.0 - u), 5.0);
}

vec3 brdf(vec3 v, vec3 l, vec3 n, vec3 diffuse_color, vec3 f0, float roughness)
{
    const vec3 h = normalize(v + l);

    const float n_dot_v = abs(dot(n, v)) + 1e-5;
    const float n_dot_l = saturate(dot(n, l));
    const float n_dot_h = saturate(dot(n, h));
    const float l_dot_h = saturate(dot(l, h));

    const float a = roughness * roughness;
    // Clamp roughness to prevent NaN fragments.
    const float a2 = max(0.01, a * a);

    const float D = D_GGX(n_dot_h, a2);
    const vec3 F = F_Schlick(l_dot_h, f0);
    const float V = V_SmithGGXCorrelated(n_dot_v, n_dot_l, a2);

    // Specular BRDF component.
    // V already contains the denominator from Cook-Torrance.
    vec3 Fr = D * V * F;

    // Diffuse BRDF component.
    vec3 Fd = diffuse_color * Fd_Lambert();
    vec3 Fin = F_Schlick(n_dot_l, f0);
    vec3 Fout = F_Schlick(n_dot_v, f0);

    // TODO: Energy conserving but causes artifacts at edges.
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#coupling-diffuse-and-specular-reflection
    // https://computergraphics.stackexchange.com/questions/2285/how-to-properly-combine-the-diffuse-and-specular-terms
    // return ((1 - Fin) * (1 - Fout) * Fd + Fr) * NoL;

    return (Fd + Fr) * n_dot_l;
}

#endif