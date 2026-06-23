# Technical Notes

This file tracks the implementation details that matter when changing HE3D.

## Backends

HE3D is built around `HE3D::Platform` in `include/he3d_platform.hpp`. The engine
does not call SDL or XAPI directly; it asks the platform layer for memory, file
access, windows, input, time, and framebuffer presentation.

Current backends:

- `src/platform/he3d_platform_xapi.cpp`: XJ380/XAPI backend
- `src/platform/he3d_platform_sdl3.cpp`: native SDL3 backend

Select one backend at configure time:

```sh
cmake -S . -B build -DHE3D_BACKEND=XAPI
cmake -S . -B build -DHE3D_BACKEND=SDL3
```

Use one `build/` directory at a time. Reconfigure it when switching backend.

## XJ380 Input

The XJ380 API manual is the source of truth for GUI message layout.

Keyboard messages use `lData` for the key value:

- `MSG_CHAR`: `hData = 0`, `lData = UTF-8 bytes`
- `MSG_SPCHAR`: `hData = 0`, `lData = special key code`

Mouse messages use window-relative coordinates:

- `MSG_MOVE`, button messages: `hData = x`, `lData = y`
- `MSG_ROLLER`: `hData = (x << 32) | y`, `lData = signed wheel delta`
- `MSG_RESIZE`: both fields are reserved; query size with `xapi_GetWindowSize`

The XAPI backend normalizes only keyboard events today. It treats XJ380 as a
pressed-only input source and synthesizes key release after a short timeout in
`XapiPollEvents`.

## Framebuffer Format

`HE3D::ColorA` is byte ordered as:

```cpp
struct ColorA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};
```

This matches XAPI `XCOLORA`, so the XAPI backend can pass the renderer buffer to
`xapi_WriteBufferA` directly. The SDL3 backend creates an `SDL_PIXELFORMAT_RGBA32`
texture for the same layout.

## Renderer

The renderer is a straightforward CPU rasterizer in `src/he3d.cpp`.

- `Renderer::Clear` fills color and depth buffers.
- `DrawGameObject(..., float3 color)` draws solid meshes.
- `DrawGameObject(..., const Texture&)` draws textured meshes.
- Depth stores `1 / z`, so larger depth values are closer.
- Mesh normals are stored one per triangle in `Mesh::triNormals`.

The renderer does near-plane rejection per triangle. It does not clip triangles
that cross the near plane; those triangles are skipped.

## Assets

The flight simulator uses:

- `examples/FlightSimulator/assets/biplane.obj`
- `examples/FlightSimulator/assets/biplane.bmp`

The build copies both files next to the executable. If the OBJ is missing,
`FLIGHT_FALLBACK_AIRCRAFT_VERTICES` provides a small built-in aircraft mesh.

`Texture::LoadBMP` supports uncompressed 24-bit and 32-bit BMP files.

## Terrain Streaming

The terrain is a fixed 3x3 tile pool around the aircraft. Tile meshes are
allocated once and then updated in place with `UpdatePlaneMesh`.

Crossing a tile boundary schedules missing tiles into a small pending list.
Only one tile is generated or refreshed per frame. This keeps XJ380 from taking
a long single-frame stall when the aircraft enters a new terrain cell.

Rendering also skips active tiles that are too far from the camera in the X/Z
plane. That keeps the steady-state terrain cost close to what is visible.

The important constants live in `examples/FlightSimulator/src/main.cpp`:

- `RD`: tile radius, currently `1`
- `GS`: grid step
- `GC`: grid vertex count per side
- `TS`: tile world size

Changing `GC` or `RD` has a direct CPU cost in rasterization and mesh updates.
