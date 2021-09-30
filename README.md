# Deferred OpenGL rendering engine

## Table of contents

1. [Showcase](#showcase)
2. [Feature list](#feature-list)
3. [Building](#building)
4. [Technical overview](#technical-overview)
5. [Development log](https://github.com/yhamdoud/engine/wiki/Development-Log)

## Showcase

https://user-images.githubusercontent.com/18217298/134210414-40b6ae4c-4609-4e9a-a16c-c40ca20de11f.mp4

https://user-images.githubusercontent.com/18217298/135535368-e1435ba9-5313-4240-aac8-15c663436d46.mp4

## Feature list

### Rendering

-   Deferred rendering
-   PBR material system
-   Irradiance probes for diffuse GI
-   Stable CSM
-   SSAO
-   HDR lighting
-   Bloom
-   Tone mapping
-   Directional and point lights
-   Normal mapping
-   Skyboxes

### Engine

-   OpenGL 4.6
    -   Direct state access (DSA)
-   Amortized GI probe baking
-   GPU probe baking
-   glTF model loading
-   DDS, PNG, JPEG texture loading
-   Transform hierarchy

### Planned

-   TAA
-   SSR
-   Glossy GI
-   Bilateral blur filter

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

## Technical overview

### Deferred rendering

The G-buffer consists of the following render targets:

-   RT0 (RGBA16F)
    -   RGB: Normal
    -   A: Metallic
-   RT1 (sRGB8):
    -   RGB: Base color
    -   A: Roughness
-   Depth buffer

### Physically based shading

For the specular BRDF, the Cook-Torrance approximation is used.
The diffuse BRDF is approximated by a simplified Lambertian model.
The implementation of both borrows from the work presented in
[Physically Based Rendering in Filament](https://google.github.io/filament/Filament.md.html) and [Real Shading in Unreal Engine 4](https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf).

The material system only supports the metallic-roughness workflow as of writing.

### Diffuse GI

Diffuse GI for static and dynamic objects is approximated using a grid of irradiance probes.
During baking, the scene is rasterized at each probe and the resulting irradiance map is processed with a compute shader.
This process is repeated for multiple bounces and can be amortized over several frames.

An efficient encoding is required to support many probes, the fact that we are primarily interested in capturing low frequency detail can be put to good use here.
We opted to project radiance map on the third-order spherical harmonics basis functions.
This allows us to represent each map using just 27 floating-point numbers, 9 for each channel to be specific.
Furthermore, calculating the irradiance ends up being a single multiplication of each coefficient by a constant factor, instead of a convolution.

In practice, the cube map projection algorithm in [Stupid Spherical Harmonics
Tricks](http://www.ppsloan.org/publications/StupidSH36.pdf) is used.
The algorithm is implemented using 2 compute shaders, one for the actual projection of each texel and the other for gathering the results with a parallel sum reduction.
The resulting coefficients are packed in 7 three-dimensional textures.
At run-time, probes are blended by hardware accelerated trilinear interpolation of the coefficients stored in each texture.

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
