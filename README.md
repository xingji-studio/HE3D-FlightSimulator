# HE3D

[中文](README.zh-CN.md)

HE3D is a C++11 software 3D renderer. The main example is a flight simulator
that can be built for XJ380/XAPI, SDL3, or a console backend.

## Build

XAPI:

```sh
cmake -S . -B build -DHE3D_BACKEND=XAPI
cmake --build build
```

SDL3:

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3
cmake --build build
```

Console:

```sh
cmake -S . -B build -DHE3D_BACKEND=CONSOLE
cmake --build build
./build/he3d_flight_simulator_console
```

Executables and copied assets are written to `build/`.

## Controls

- `W` / `S`: pitch
- `Q` / `E`: yaw
- `A` / `D`: roll
- `F`: toggle FXAA
- `Esc`: quit

## Project Layout

- `include/`: public engine headers
- `src/`: renderer, loaders, and platform backends
- `examples/FlightSimulator/`: flight simulator example
- `xj380/include/`: XJ380 headers used by the XAPI build
- [docs/technical.md](docs/technical.md): technical manual and API reference

## License

[MIT](LICENSE)
