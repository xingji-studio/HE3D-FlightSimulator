# HE3D

[中文](README.zh-CN.md)

HE3D is a C++11 software-rendered 3D engine with a small portable platform layer. The same flight simulator example can build for XJ380/XAPI or native SDL3.

## Features

- Software triangle rasterizer with Z-buffer depth testing
- OBJ mesh loading for the biplane model
- 24-bit uncompressed BMP texture loading
- Directional lighting for solid and textured meshes
- Quaternion-based flight orientation
- Smooth chase camera
- Procedural terrain tiles with recycling around the aircraft
- Procedural fallback airplane mesh if `biplane.obj` cannot be loaded
- Portable platform interface for memory, file loading, windows, input, timing, and framebuffer presentation

## Controls

- `W` / `S`: pitch
- `Q` / `E`: yaw
- `A` / `D`: roll
- `Esc`: quit

## Project Layout

- `include/`: HE3D engine public headers
- `include/he3d.hpp`: engine class interfaces
- `include/he3d_platform.hpp`: LVGL-style platform interface
- `include/he3d_math.h`: compact math library for vectors, quaternions, and scalar math
- `src/he3d.cpp`: mesh initialization, OBJ/BMP loading, renderer, rasterizer
- `src/platform/he3d_platform_xapi.cpp`: XJ380/XAPI backend
- `src/platform/he3d_platform_sdl3.cpp`: SDL3 backend
- `examples/FlightSimulator/`: flight simulator example
- `examples/FlightSimulator/src/`: flight simulator entry point and terrain mesh setup
- `examples/FlightSimulator/assets/`: aircraft mesh and texture assets copied next to the executable after build
- `xj380/include/`: XJ380 API and compatibility headers

## Build

The project uses CMake and Ninja. Release builds use GCC with `-O2`; Debug builds use Clang with debug symbols.

```sh
cmake -S . -B build/release-xapi -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DHE3D_BACKEND=XAPI
cmake --build build/release-xapi

cmake -S . -B build/release-sdl3 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DHE3D_BACKEND=SDL3
cmake --build build/release-sdl3

cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DHE3D_BACKEND=XAPI
cmake --build build/debug
```

Select exactly one backend with `-DHE3D_BACKEND=XAPI` or `-DHE3D_BACKEND=SDL3`. The default is `XAPI`.

Targets for `XAPI`:

- `he3d_engine_xapi`: engine core plus XAPI backend
- `he3d_flight_simulator.elf`: XJ380/XAPI flight simulator

Targets for `SDL3`:

- `he3d_engine_sdl3`: engine core plus SDL3 backend
- `he3d_flight_simulator_sdl3`: native SDL3 flight simulator

## Clean

```sh
cmake --build build/release-xapi --target clean
cmake --build build/release-sdl3 --target clean
cmake --build build/debug --target clean
```
