#ifndef MATH_H
#define MATH_H

#define PI 3.14159265359

// clang-format off
const mat4 bayer = mat4(
    0.,  8.,  2.,  10.,
    12., 4.,  14., 6.,
    3.,  11., 1.,  9.,
    15., 7.,  13., 5.
) / 16.;
// clang-format on

float blackman_harris(float x)
{
    x = 1. - x;

    // https://en.wikipedia.org/wiki/Window_function#Blackman%E2%80%93Harris_window
    const float a0 = 0.35875;
    const float a1 = 0.48829;
    const float a2 = 0.14128;
    const float a3 = 0.01168;

    return saturate(a0 - a1 * cos(PI * x) + a2 * cos(2 * PI * x) -
                    a3 * cos(3 * PI * x));
}

float mitchell(float x)
{
    const float B = 1. / 3.;
    const float C = 1. / 3.;

    float y = 0.0f;
    float x2 = x * x;
    float x3 = x * x * x;
    if (x < 1)
        y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 +
            (6 - 2 * B);
    else if (x <= 2)
        y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x +
            (8 * B + 24 * C);

    return y / 6.0f;
}

float henyey_greenstein(float cos_theta, float g)
{
    return (1. - g * g) / (4. * PI * pow(1. + g * g - 2. * g * cos_theta, 1.5));
}

#endif