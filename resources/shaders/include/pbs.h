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
float Fd_Lambert()
{
    return 1.0 / PI;
}

// Source: https://seblagarde.wordpress.com/2011/08/17/hello-world/
vec3 F_Schlick_roughness(float u, vec3 f0, float roughness)
{
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - u, 0.0, 1.0), 5.0);
}   

#endif