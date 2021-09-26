# Deferred OpenGL rendering engine

## Showcase

https://user-images.githubusercontent.com/18217298/134210414-40b6ae4c-4609-4e9a-a16c-c40ca20de11f.mp4

[Have a look at the development log in the wiki.](https://github.com/yhamdoud/engine/wiki/Development-Log)

## Features

### Rendering

-   Deferred rendering
-   Physically based shading
-   Global illumination
    -   Irradiance probes
-   Stable cascaded shadow mapping
-   Screen space ambient occlusion
-   Bloom
-   Tangent space normal mapping
-   Tonemapping
-   Directional and point lights
-   Skyboxes

### Engine

-   OpenGL 4.6
    -   Direct state access (DSA)
-   GPU probe baking
-   glTF model loading
-   Transform hierarchy

## Planned features

-   TAA
-   SSR
-   Glossy GI

## Build

This project uses CMake for
building. [Investigate on how to install CMake on your platform.](https://cmake.org/install/)
All other dependencies are vendored directly or as submodules:

-   [glad](https://github.com/Dav1dde/glad): for loading OpenGL functions at runtime.
-   [GLFW](https://github.com/glfw/glfw): for windowing and input.
-   [GLM](https://github.com/g-truc/glm): for linear algebra and other math.
-   [cgtlf](https://github.com/jkuhlmann/cgltf): for glTF model loading.
-   [stb](https://github.com/nothings/stb): for image loading.
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

You can use the Windows equivalent of the steps shown above. Alternatively, you might use the CMake GUI or Visual
Studio's CMake integration.
