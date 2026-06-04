# HE3D Flight Simulator

[中文](README.zh-CN.md)

HE3D Flight Simulator is a C++11 software-rendered 3D flight demo for the XJ380/XACT platform. It uses the XJ380 window and file APIs directly, without the C++ standard library runtime or BridgeEngine in the current `src/` implementation.

## Features

- Software triangle rasterizer with Z-buffer depth testing
- OBJ mesh loading for the biplane model
- 24-bit uncompressed BMP texture loading
- Directional lighting for solid and textured meshes
- Quaternion-based flight orientation
- Smooth chase camera
- Procedural terrain tiles with recycling around the aircraft
- Procedural fallback airplane mesh if `Biplane.obj` cannot be loaded

## Controls

- `W` / `S`: pitch
- `Q` / `E`: yaw
- `A` / `D`: roll
- `Esc`: quit

## Project Layout

- `src/he3d.hpp`: engine class interfaces
- `src/he3d.cpp`: generic mesh initialization, OBJ/BMP loading, renderer, rasterizer
- `src/he3d_math.h`: compact math library for vectors, quaternions, and scalar math
- `src/flight_sim.hpp` and `src/flight_sim.cpp`: flight simulator data and terrain tile mesh setup
- `src/main.cpp`: flight simulation entry point and main loop
- `include/`: XJ380 API and compatibility headers
- `Biplane.obj`: aircraft mesh asset
- `biplane.bmp`: aircraft texture asset
- `OldSrc/`: older SDL-based prototype kept for reference

## Build

The main build uses the XJ380 XACT `xxcc` compiler and C++11:

```sh
make
```

Ensure `xxcc` is available in your `PATH` before running `make`. Users must provide or build the XJ380 `obj-gui` and `obj-tui` runtime objects themselves.

## Clean

```sh
make clean
```

## Notes

`xxcc -c` is noted as unstable in this environment, so the Makefile compiles and links in one command.
