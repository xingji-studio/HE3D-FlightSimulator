# HE3D

[中文](README.zh-CN.md)

HE3D is a C++11 software 3D renderer. The main sample is a flight simulator
that builds for XJ380/XAPI or SDL3.

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

The executable and copied assets are written to `build/`.

## Controls

- `W` / `S`: pitch
- `Q` / `E`: yaw
- `A` / `D`: roll
- `Esc`: quit

## Layout

- `include/`: public engine headers
- `src/`: renderer, loaders, and platform backends
- `examples/FlightSimulator/`: flight simulator sample
- `xj380/include/`: XJ380 headers for the XAPI build
- `docs/technical.md`: implementation notes
