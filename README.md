# Deferred OpenGL rendering engine

https://user-images.githubusercontent.com/18217298/137032993-6adf4c04-9362-4703-beb4-0e1047f122d1.mp4

## Table of contents

1. [Feature list](#feature-list)
2. [Building](#building)
3. [Showcase](#showcase)
4. [Technical overview](#technical-overview)
5. [Development log](https://github.com/yhamdoud/engine/wiki/Development-Log)

## Feature list

### Rendering

-   Deferred rendering
-   PBR material system
-   Irradiance probes for diffuse GI
-   Stable CSM
-   SSR
-   SSAO
-   Bloom
-   HDR lighting
-   Automatic exposure
-   Tone mapping
-   Motion blur
-   Directional and point lights
-   Normal mapping
-   Skyboxes

### Engine

-   OpenGL 4.6
-   Direct state access (DSA)
-   Amortized probe baking
-   GPU and CPU probe baking
-   glTF model loading
-   DDS, PNG, JPEG texture loading
-   Transform hierarchy
-   GPU profiler
-   CPU profiling with Tracy
-   Editor UI

### Planned

-   TAA
-   Reflection probes
-   Light volumes
-   Bilateral blur filter
-   Volumetric lighting

## Building

This project uses CMake for building.
[Investigate on how to install CMake on your platform.](https://cmake.org/install/)
All other dependencies are vendored directly or as submodules:

-   [glad](https://github.com/Dav1dde/glad): for loading OpenGL functions at runtime.
-   [GLFW](https://github.com/glfw/glfw): for windowing and input.
-   [GLM](https://github.com/g-truc/glm): for linear algebra and other math.
-   [GLI](https://github.com/g-truc/gli): for (compressed) DDS image loading.
-   [cgtlf](https://github.com/jkuhlmann/cgltf): for glTF model loading.
-   [stb](https://github.com/nothings/stb): for PNG and JPEG image loading.
-   [Dear ImGui](https://github.com/ocornut/imgui): for user interface.
-   [fmt](https://github.com/fmtlib/fmt): for hassle-free string formatting.
-   [Tracy](https://github.com/fmtlib/fmt): for CPU and GPU profiling.

### Linux

```shell
$ git clone --recursive https://github.com/yhamdoud/engine.git
$ mkdir engine/build
$ cd engine/build
$ cmake ..
$ make
$ ./engine
```

### Windows

The Windows equivalent of the steps shown above can be used.
Alternatively, the CMake GUI or Visual Studio's CMake integration could be used.

## Showcase

### Baking

https://user-images.githubusercontent.com/18217298/136076580-9459b416-ab55-4d88-a427-2f2645f74d3f.mp4

### Reflections

https://user-images.githubusercontent.com/18217298/136068154-a21cba37-ff7b-4947-a607-21e0bccd24e1.mp4

### Tone mapping

https://user-images.githubusercontent.com/18217298/137027876-c6458eea-c5b4-4d22-aa5b-5ba6a5e40fd6.mp4

## Technical overview

### Deferred rendering

The G-buffer consists of the following render targets:

-   RT0 (RGBA16F)
    -   RGB: Normal
    -   A: Metallic
-   RT1 (SRGB8_ALPHA8)
    -   RGB: Base color
    -   A: Roughness
-   RT2 (RGBA16F)
    -   RG: Velocity
    -   BA: Unused
-   Depth buffer

### Physically based shading

The Cook-Torrance model is used as specular BRDF.
The diffuse BRDF is approximated by a simplified Lambertian model.
The implementation of both borrows from the work presented in
[Physically Based Rendering in Filament](https://google.github.io/filament/Filament.md.html) and [Real Shading in Unreal Engine 4](https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf).

The material system only supports the metallic-roughness workflow as of writing.

### Diffuse indirect lighting

Diffuse indirect lighting for static and dynamic objects is approximated using a grid of irradiance probes.
For each probe, the scene is rasterized to a cube map and the irradiance is subsequently determined using a compute shader.
This process is repeated for multiple bounces and can be amortized over several frames.

An efficient encoding for irradiance is desirable when storing many probes, the fact that we are primarily interested in capturing low frequency detail can be put to good use here.
We opted to project the radiance map on the third-order spherical harmonics basis functions.
This allows us to represent each map using just 27 floating-point numbers (9 for each channel).
Furthermore, calculating the irradiance ends up being a multiplication of each coefficient by a constant factor, instead of a convolution.

In practice, the cube map projection algorithm presented in [Stupid Spherical Harmonics
Tricks](http://www.ppsloan.org/publications/StupidSH36.pdf) is used.
The algorithm is implemented using 2 compute shaders, one for the actual projection of each cube map texel and the other for reducing the results of the first pass in parallel.
The resulting coefficients are packed in 7 three-dimensional textures and sampled at run-time using hardware accelerated trilinear interpolation.

### Screen-space reflections

The view space position of the fragment is reconstructed from the depth buffer and corresponding surface normal is sampled from the g-buffer.
A ray leaving the camera is reflected about this normal.
The reflected ray is used to march the depth buffer.
Given that it's visible on screen, we end up with an approximate hit position, which can be used to sample a buffer containing the lighted scene.

The implemented depth buffer marching solution relies on rasterization of the ray in screen-space using a DDA algorithm, as first presented by [Morgan McGuire and Mike Mara](https://casual-effects.blogspot.com/2014/08/screen-space-ray-tracing.html).
This technique evenly distributes the samples, and thus avoids over- and undersampling issues.

### Screen-space ambient occlusion

The SSAO pass uses a normal-oriented hemisphere for sampling.
A tiling texture is used to randomly offset the sample kernel, as described in [this blog post](https://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html).
The SSAO texture is box blurred in a full-screen post-processing pass.

The editor exposes several parameters, like sample count and radius.
This allows one to tweak the SSAO for a specific scene.

### Bloom

The bloom post-processing pass consists of four steps:

1. Downsampling and filtering the HDR render target.
2. Upsampling and filtering the downsampled image.
3. Additively combining the upsampled images.
4. Blending the resulting texture with the original HDR target.

Upsampling and downsampling happens progressively and results in a mip chain of textures.
For clarification, we refer to presentation that was referenced for this implementation: [Next Generation Post Processing in Call of Duty: Advanced Warfare](http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare).

The upsampling and downsampling passes are implemented as compute shaders.
The following filters are available:

-   13-tap downsampling filter, from the aforementioned presentation.
-   16-texel average downsampling filter.
-   3x3 tent upsampling filter.

Additionally, a specialized version of the 13-tap filter is implemented for the first downsample pass to reduce fireflies.

### Tone mapping

The first part of the tone mapping process is calculating the average luminance for automatic exposure adjustment.
A straightforward way of calculating this average is to successively downsample the HDR target into a 1×1 luminance buffer.
However, the stability of this approach can suffer from the presence of very bright and dark pixels in the HDR target, which over-contribute to the average luminance.

A more robust approach for calculating the average luminance relies on the creation of a luminance histogram.
Binning samples in a histogram gives us more control over the impact of samples at both extremes.
Our histogram construction algorithm is based on the work presented by Alex Tarfif in [Adaptive Exposure from Luminance Histograms](http://www.alextardif.com/HistogramLuminance.html).
The implementation consists of two compute shader passes: one for creating the histogram, and one for calculating the mean.
In practice, we bin log luminance values and calculate the average log luminance.
This limits the effect of extremely bright spots, like the sun, on adjustment.
Additionally, the averaged value is smoothed over time.
