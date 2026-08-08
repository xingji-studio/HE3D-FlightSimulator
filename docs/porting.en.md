# HE3D Porting Guide

[中文版](porting.md)

This document explains how to port HE3D to a new platform. The core task is not changing the renderer; it is implementing a platform backend.

## Backend Responsibilities

A new backend implements `HE3D::Platform`:

| Capability | Description |
| --- | --- |
| Memory | `alloc` / `free`, used by HE3D global `new/delete`. |
| Files | `loadFile` / `closeFile`, used by OBJ, BMP, PNG, and JPG loaders. |
| Window | Create, destroy, and set title; on bare platforms Window can be a framebuffer handle. |
| Input | `setKeyCallback` and `pollEvents` deliver key-down/key-up. |
| Time | `timeSeconds` returns monotonic seconds; `sleepMilliseconds` supports frame pacing. |
| Display | `present` submits the `ColorA` framebuffer to the platform. |

## Minimal Port Target

The first backend version should only run `TriangleTest`:

1. Implement memory, time, window, and `Present()`.
2. Use built-in meshes first; do not load OBJ/images yet.
3. Support only `Esc` quit, or skip quitting at first.
4. Start with a small resolution such as `160x120` or `320x240`.

File loading, textures, complex input, and physics examples can come later.

## Present

`Present()` is the key porting function. HE3D calls:

```cpp
void present(Window *window, int32_t width, int32_t height, const ColorA *pixels);
```

`pixels` is an RGBA 8-bit array. The backend converts it to the target framebuffer format, for example:

- SDL texture.
- Terminal characters.
- XJ380 `ColorA` buffer.
- UEFI GOP framebuffer.
- TFT RGB565 buffer.

If render size and screen size differ, center the image or use nearest-neighbor scaling for the first version.

## UEFI

UEFI fits HE3D's software rendering model well:

- `Alloc/Free` map to Boot Services pool allocation.
- `Window` stores GOP framebuffer information.
- `Present()` converts and copies according to GOP pixel format.
- `SleepMilliseconds()` can use `Stall()`.
- File loading can use the Simple File System Protocol.

The first UEFI target should be a built-in triangle or cube, not FlightSimulator.

## Arduino and MCU

AVR boards such as Arduino Uno/Nano have too little RAM for full HE3D. ESP32, Teensy, and RP2040 can run a trimmed build:

- Low resolution.
- Disable MSAA/TAA/SSAA.
- Avoid large textures and runtime OBJ loading.
- Prefer built-in meshes or compile-time assets.
- Avoid frequent `malloc/free`.

## Backend Checklist

- `CreateWindow()` returns `nullptr` on failure.
- `Present()` never writes outside the framebuffer.
- `PollEvents()` can run every frame and does not block the main loop.
- Platforms with key-up deliver real key-up; platforms without key-up synthesize release.
- `TimeSeconds()` is monotonic.
- `LoadFile()` data remains valid until `CloseFile()`.
