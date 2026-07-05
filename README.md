# HE3D

[中文 README](README.zh-CN.md)

HE3D is a C++11 software 3D renderer with small examples and a larger flight
simulator example. The same engine code can be built for XJ380/XAPI, SDL3, or a
console backend.

For a first project, start with [Getting Started](docs/getting-started.en.md).

## Build

Choose the backend with `HE3D_BACKEND` and the example with `HE3D_EXAMPLE`.

SDL3 TriangleTest:

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=TriangleTest
cmake --build build
./build/he3d_triangle_test_sdl3
```

SDL3 Flight Simulator:

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=FlightSimulator
cmake --build build
./build/he3d_flight_simulator_sdl3
```

SDL3 AirplaneTest:

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=AirplaneTest
cmake --build build
./build/he3d_airplane_test_sdl3
```

SDL3 PhysicsTest:

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=PhysicsTest
cmake --build build
./build/he3d_physics_test_sdl3
```

Console:

```sh
cmake -S . -B build -DHE3D_BACKEND=CONSOLE -DHE3D_EXAMPLE=TriangleTest
cmake --build build
./build/he3d_triangle_test_console
```

XAPI:

```sh
cmake -S . -B build -DHE3D_BACKEND=XAPI -DHE3D_EXAMPLE=FlightSimulator
cmake --build build
```

For XAPI, build or provide the XJ380 GUI runtime objects first. CMake searches `../XXCC-suite/obj-gui` and `../XJ380/out/xapi`, or use `-DXJ380_GUI_RUNTIME_DIR=/path/to/runtime`.

Executables and copied assets are written to `build/`.

## Flight Simulator Example Controls

- `W` / `S`: pitch
- `Q` / `E`: yaw
- `A` / `D`: roll
- `F`: toggle FXAA
- `Esc`: quit

## Project Layout

- `include/`: public engine headers
- `src/`: renderer, loaders, and platform backends
- `tests/`: test layout and boundaries
- `examples/TriangleTest/`: smallest example
- `examples/AirplaneTest/`: OBJ airplane model loading example
- `examples/PhysicsTest/`: physics and collision example
- `examples/FlightSimulator/`: flight simulator example
- `xj380/include/`: XJ380 headers used by the XAPI build
- [docs/getting-started.en.md](docs/getting-started.en.md): beginner guide
- [docs/technical.en.md](docs/technical.en.md): technical manual and API reference

## License

[MIT](LICENSE)
