# Deferred OpenGL rendering engine

## Showcase

[Have a look at the development log in the wiki.]( https://github.com/yhamdoud/engine/wiki/Development-Log)

## Features

### Rendering

- Deferred shading
- Physically based shading
- Shadow mapping
- Tangent space normal mapping
- Skyboxes
- Point lights

### Engine

- OpenGL 4.6
    - Direct state access (DSA)
- glTF model loading
- Transform hierarchy

## Planned features

- TAA
- SSAO
- Tonemapping
- Bloom
- SSR
- GI

## Build

This project uses CMake for
building. [Investigate on how to install CMake on your platform.](https://cmake.org/install/)
All other dependencies are vendored directly or as submodules:

- [glad](https://github.com/Dav1dde/glad): for loading OpenGL functions at runtime.
- [GLFW](https://github.com/glfw/glfw): for windowing and input.
- [GLM](https://github.com/g-truc/glm): for linear algebra and other math.
- [stb](https://github.com/nothings/stb): for image loading.
- [Dear ImGui](https://github.com/ocornut/imgui): for user interface.
- [fmt](https://github.com/fmtlib/fmt): for hassle-free string formatting.
- [Tracy](https://github.com/fmtlib/fmt): for CPU and GPU profiling.

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
