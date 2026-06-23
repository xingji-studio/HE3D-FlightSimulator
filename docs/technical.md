# HE3D Technical Manual

This document covers the public interfaces shipped by HE3D: build options,
headers, platform services, math types, renderer objects, asset formats, and
the flight simulator sample.

## Build

HE3D builds one backend at a time through CMake.

```sh
cmake -S . -B build -DHE3D_BACKEND=XAPI
cmake --build build
```

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3
cmake --build build
```

`HE3D_BACKEND` accepts:

- `XAPI`: builds `he3d_flight_simulator.elf` for XJ380.
- `SDL3`: builds `he3d_flight_simulator_sdl3` for desktop SDL3.

Generated executables and copied assets are placed in `build/`. Reconfigure the
same `build/` directory when switching backend.

## Public Headers

- `include/he3d.hpp`: engine objects, renderer, mesh, texture, camera, light.
- `include/he3d_platform.hpp`: platform interface used by the engine.
- `include/he3d_math.h`: scalar math, vectors, quaternions, and matrix type.

All public C++ types are in namespace `HE3D`. The global `new` and `delete`
operators are provided by HE3D and forward allocation to the active platform.

## Math API

`he3d_math.h` is freestanding and does not require libm.

Scalar functions:

- `fabsf(float x) -> float`
- `abs(int x) -> int`
- `floorf(float x) -> float`
- `ceilf(float x) -> float`
- `roundf(float x) -> float`
- `fracf(float x) -> float`
- `sqrtf(float x) -> float`
- `rsqrtf(float x) -> float`
- `sinf(float x) -> float`
- `cosf(float x) -> float`
- `tanf(float x) -> float`
- `sincosf(float x, float *s, float *c)`
- `atan2f(float y, float x) -> float`

Macros and constants:

- `HE3D_MIN(a, b)`
- `HE3D_MAX(a, b)`
- `HE3D_CLAMP(x, lo, hi)`
- `HE3D_ABS(x)`
- `HE3D_DEG2RAD(d)`
- `HE3D_PI`
- `HE3D_TAU`
- `HE3D_PI_DIV_2`

`float2` stores two scalar values:

```cpp
struct float2 {
    float x, y;
};
```

Supported operations:

- constructor: `float2(float x = 0, float y = 0)`
- `+`, `-`
- scalar `*` and `/`
- component-wise `*`

`float3` stores a vector or RGB color:

```cpp
struct float3 {
    union {
        struct { float x, y, z; };
        struct { float r, g, b; };
    };
};
```

Supported operations:

- constructor: `float3(float x = 0, float y = 0, float z = 0)`
- `+`, `-`, unary `-`
- scalar `*` and `/`
- component-wise `*`
- `lengthSq()`
- `length()`
- `normalize()`
- `normalizeFast()`
- `float3::dot(a, b)`
- `float3::cross(a, b)`
- `float3::lerp(a, b, t)`
- `rotateX(angle)`, `rotateY(angle)`, `rotateZ(angle)`
- `rotateX(s, c)`, `rotateY(s, c)`, `rotateZ(s, c)`

`quat` represents rotation:

```cpp
struct quat {
    float w, x, y, z;
};
```

Supported operations:

- constructor: `quat(float w = 1, float x = 0, float y = 0, float z = 0)`
- `quat::FromEuler(float3 euler)`
- `quat::FromEulerFast(float3 euler)`
- `normalize()`
- `normalizeFast()`
- quaternion multiplication with `operator*`
- `rotate(const float3& v)`
- `inverse()`

Euler angles are in radians. `FromEuler` uses X as pitch, Y as yaw, and Z as
roll.

`float4x4` is a 4x4 identity-initialized matrix:

```cpp
struct float4x4 {
    float m[16];
};
```

## Platform API

The platform layer supplies memory, file access, windows, input, time, and
framebuffer presentation.

Pixel layout:

```cpp
struct ColorA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};
```

Loaded file data:

```cpp
struct FileData {
    void *handle;
    const unsigned char *data;
    unsigned long long length;
};
```

Window creation:

```cpp
struct Window;

struct WindowDesc {
    int width;
    int height;
    const char *title;
    unsigned int flags;
};
```

Keyboard callbacks:

```cpp
typedef void (*KeyCallback)(int key, bool pressed, void *user);
```

Platform function table:

```cpp
struct Platform {
    void *(*alloc)(unsigned long size);
    void  (*free)(void *ptr);

    bool  (*loadFile)(const char *path, FileData *outFile);
    void  (*closeFile)(FileData *file);

    Window *(*createWindow)(const WindowDesc *desc);
    void    (*setWindowTitle)(Window *window, const char *title);
    void    (*destroyWindow)(Window *window);
    void    (*setKeyCallback)(Window *window, KeyCallback callback, void *user);
    void    (*pollEvents)(Window *window);
    bool    (*shouldClose)(Window *window);
    double  (*timeSeconds)();
    void    (*present)(Window *window, int width, int height, const ColorA *pixels);
};
```

Platform entry points:

- `GetPlatform() -> const Platform *`
- `SetPlatform(const Platform *platform)`
- `GetBuiltinPlatform() -> const Platform *`
- `Alloc(unsigned long size) -> void *`
- `Free(void *ptr)`
- `LoadFile(const char *path, FileData *outFile) -> bool`
- `CloseFile(FileData *file)`
- `CreateWindow(const WindowDesc *desc) -> Window *`
- `SetWindowTitle(Window *window, const char *title)`
- `DestroyWindow(Window *window)`
- `SetKeyCallback(Window *window, KeyCallback callback, void *user)`
- `PollEvents(Window *window)`
- `WindowShouldClose(Window *window) -> bool`
- `TimeSeconds() -> double`
- `Present(Window *window, int width, int height, const ColorA *pixels)`

Files returned by `LoadFile` remain valid until `CloseFile` is called. Windows
returned by `CreateWindow` must be released with `DestroyWindow`.

## Custom Backend

A backend is one translation unit that provides a `Platform` table and, when it
is the built-in backend for a target, defines `HE3D::GetBuiltinPlatform()`.

There are two supported ways to use a custom backend:

- Link it as the target backend by defining `GetBuiltinPlatform()` in the custom
  backend source file. Do not link another source file that also defines
  `GetBuiltinPlatform()`.
- Link an existing backend and call `SetPlatform(&myPlatform)` before creating a
  window, loading assets, or allocating engine objects.

Backend files normally use this shape:

```cpp
#include "he3d_platform.hpp"

namespace HE3D {

struct Window {
    /* backend-owned window state */
};

static void *MyAlloc(unsigned long size) { /* ... */ }
static void  MyFree(void *ptr) { /* ... */ }

static bool MyLoadFile(const char *path, FileData *outFile) { /* ... */ }
static void MyCloseFile(FileData *file) { /* ... */ }

static Window *MyCreateWindow(const WindowDesc *desc) { /* ... */ }
static void MySetWindowTitle(Window *window, const char *title) { /* ... */ }
static void MyDestroyWindow(Window *window) { /* ... */ }
static void MySetKeyCallback(Window *window, KeyCallback callback, void *user) { /* ... */ }
static void MyPollEvents(Window *window) { /* ... */ }
static bool MyShouldClose(Window *window) { /* ... */ }
static double MyTimeSeconds() { /* ... */ }
static void MyPresent(Window *window, int width, int height, const ColorA *pixels) { /* ... */ }

static const Platform g_myPlatform = {
    MyAlloc,
    MyFree,
    MyLoadFile,
    MyCloseFile,
    MyCreateWindow,
    MySetWindowTitle,
    MyDestroyWindow,
    MySetKeyCallback,
    MyPollEvents,
    MyShouldClose,
    MyTimeSeconds,
    MyPresent
};

const Platform *GetBuiltinPlatform()
{
    return &g_myPlatform;
}

}
```

Backend callback contracts:

- `alloc` returns storage suitable for any HE3D object. `free` releases storage
  from `alloc` and should accept `nullptr`.
- `loadFile` sets `outFile->handle`, `outFile->data`, and `outFile->length` on
  success. It returns `false` and leaves no owned data on failure.
- `closeFile` releases data returned by `loadFile`.
- `createWindow` returns a backend-owned `Window *`. `Window` is opaque outside
  the backend.
- `setWindowTitle` may ignore unsupported title changes, but must tolerate a
  valid window and title.
- `destroyWindow` releases all resources owned by the window.
- `setKeyCallback` stores the callback and user pointer for later input events.
- `pollEvents` pumps the host event queue and invokes the stored key callback.
- `shouldClose` returns `true` after user close or backend failure.
- `timeSeconds` returns monotonic seconds.
- `present` receives `width * height` row-major `ColorA` pixels. The backend
  does not own this memory after the call returns.

Keyboard callbacks use integer key values. Printable keys should use their ASCII
code, and Escape should use `27`. Backends with separate key-up events should
forward both press and release. Backends that only receive key press messages can
synthesize release in `pollEvents`.

`ColorA` pixels are RGBA byte order. If the host API uses a different byte order
or stride, convert or upload accordingly inside `present`.

For a CMake target, compile `src/he3d.cpp`, the custom backend source file, and
the application source. Link exactly one backend implementation that provides
`GetBuiltinPlatform()`.

## Engine API

`DirectionalLight` controls simple directional lighting:

```cpp
struct DirectionalLight {
    float3 direction;
    float3 color;
    float ambient;
};
```

Default values are direction `{0, -1, 1}`, color `{1, 1, 1}`, and ambient
`0.15f`.

`Mesh` owns triangle vertex data:

```cpp
class Mesh {
public:
    float3 *vertices;
    float2 *uvs;
    float3 *triNormals;
    int vertCount;
    int capacity;
};
```

Mesh functions:

- `bool Init(int vertexCount)`
- `bool Init(const float3 *srcVertices, const float2 *srcUvs, int vertexCount)`
- `void RecalculateTriangleNormals()`
- `static Mesh *Create(int vertexCount)`
- `static Mesh *Create(const float3 *srcVertices, const float2 *srcUvs, int vertexCount)`
- `static Mesh *LoadOBJ(const char *filename)`

`vertCount` is the active vertex count. HE3D draws triangles, so mesh data is
interpreted as groups of three vertices. `capacity` is the allocated vertex
count and allows callers to refill a mesh in place.

`Texture` owns floating-point RGB pixels:

```cpp
class Texture {
public:
    int width;
    int height;
    float3 *pixels;
    bool valid;
};
```

Texture functions:

- `float3 Sample(float u, float v) const`
- `static Texture *LoadBMP(const char *filename)`

`Sample` wraps UV coordinates and uses nearest-neighbor sampling. Invalid
textures sample as magenta `{1, 0, 1}`.

`GameObject` binds a mesh to a transform:

```cpp
class GameObject {
public:
    Mesh *mesh;
    float3 position;
    quat orientation;
};
```

GameObject functions:

- `float3 Forward() const`

`Forward()` returns local `{0, 0, 1}` transformed by `orientation`.

`Camera` stores a view transform and field of view:

```cpp
class Camera {
public:
    float3 position;
    quat orientation;
    float fov;
    float targetFov;
    float zoomSpeed;
};
```

Camera functions:

- `void Update(float deltaTime)`

`Update` moves `fov` toward `targetFov` using `zoomSpeed`.

`Renderer` draws meshes into a platform window:

```cpp
class Renderer {
public:
    DirectionalLight mainLight;

    Renderer(Window *window, int w, int h);
    ~Renderer();

    void Clear(float3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, float3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, const Texture& tex);
    void Present();
    void Resize(int w, int h);
    void SetMainLight(const DirectionalLight& light);
};
```

Call `Clear`, draw all objects, then call `Present` once per frame. `Resize`
reallocates the color and depth buffers.

## Asset Formats

`Mesh::LoadOBJ` supports common static OBJ mesh data:

- `v x y z`
- `vt u v`
- polygon faces using vertex and optional texture-coordinate indices
- fan triangulation for faces with more than three vertices

Normals from OBJ files are not required. HE3D recalculates one normal per output
triangle.

`Texture::LoadBMP` supports:

- uncompressed BMP
- 24-bit BGR
- 32-bit BGRA
- top-down and bottom-up row order
- maximum dimension of 8192 pixels per side

## Built-in Backends

The XAPI backend creates XJ380 GUI windows and presents `ColorA` buffers through
`xapi_WriteBufferA`. XJ380 keyboard messages are normalized from `MSG_CHAR` and
`MSG_SPCHAR`; the key value is read from `lData`. XJ380 sends key press messages,
so the backend synthesizes key release after a short timeout.

The SDL3 backend creates an SDL window, renderer, and streaming
`SDL_PIXELFORMAT_RGBA32` texture. SDL key down/up events are passed directly to
the HE3D key callback.

## Flight Simulator Sample

The sample lives in `examples/FlightSimulator/`.

Controls:

- `W` / `S`: pitch
- `Q` / `E`: yaw
- `A` / `D`: roll
- `Esc`: quit

Assets:

- `examples/FlightSimulator/assets/biplane.obj`
- `examples/FlightSimulator/assets/biplane.bmp`

The build copies these files next to the executable. If `biplane.obj` cannot be
loaded, the sample uses its built-in fallback aircraft mesh.

The sample keeps a 3x3 terrain tile set around the aircraft. Terrain meshes are
updated in place, and at most one missing tile is generated per frame after the
aircraft enters a new tile coordinate.

## Ownership Rules

- Objects returned by `Mesh::Create` and `Mesh::LoadOBJ` are released with
  `delete`.
- Objects returned by `Texture::LoadBMP` are released with `delete`.
- File data returned by `LoadFile` is released with `CloseFile`.
- Windows returned by `CreateWindow` are released with `DestroyWindow`.
- `GameObject` and `Camera` do not own meshes or textures.
