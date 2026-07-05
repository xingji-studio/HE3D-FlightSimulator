# HE3D Technical Manual

[中文版](technical.md)

This manual documents HE3D by public header and call contract. Applications usually include:

```cpp
#include "he3d.hpp"
```

`he3d.hpp` includes `he3d_math.h` and `he3d_platform.hpp`. Except for global `new` / `delete`, public C++ symbols are in namespace `HE3D`.

`he3d.hpp` defines global `operator new`, `operator new[]`, `operator delete`, and `operator delete[]`; they forward to the active HE3D platform allocator. Make sure the platform backend is available before creating HE3D objects.

## 1. Build

HE3D builds one backend and one example at a time:

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=TriangleTest
cmake --build build
```

| Option | Values | Default | Purpose |
| --- | --- | --- | --- |
| `HE3D_BACKEND` | `XAPI`, `SDL3`, `CONSOLE` | `XAPI` | Selects the platform backend. |
| `HE3D_EXAMPLE` | `TriangleTest`, `AirplaneTest`, `PhysicsTest`, `FlightSimulator` | `TriangleTest` | Selects the example application. |
| `XJ380_SDK_ROOT` | path | `../XXCC-suite` | Root used to find legacy XJ380 runtime objects. |
| `XJ380_GUI_RUNTIME_DIR` | path | empty | Explicit XJ380 GUI runtime object directory. |

| Backend | CMake target | Output |
| --- | --- | --- |
| XAPI | `he3d_xapi_app` | `build/he3d_<example>.elf` |
| SDL3 | `he3d_sdl3_app` | `build/he3d_<example>_sdl3` |
| CONSOLE | `he3d_console_app` | `build/he3d_<example>_console` |

The XAPI build links prebuilt XJ380 GUI runtime objects. Search order:

1. `-DXJ380_GUI_RUNTIME_DIR=/path/to/runtime`
2. `${XJ380_SDK_ROOT}/obj-gui`
3. `../XJ380/out/xapi`

When `../XJ380/out/xapi` is used, `constart.cpp.o` is excluded so the console startup object does not collide with the GUI application entry.

## 2. Minimal Program

```cpp
#include "he3d.hpp"

static bool quit = false;

static void OnKey(HE3D::int32_t key, bool pressed, void *) {
    if (key == 27 && pressed) quit = true;
}

int main() {
    HE3D::WindowDesc desc;
    desc.width = 640;
    desc.height = 360;
    desc.title = "HE3D App";
    desc.flags = 0;

    HE3D::Window *window = HE3D::CreateWindow(&desc);
    if (!window) return 1;

    HE3D::SetKeyCallback(window, OnKey, nullptr);
    HE3D::SetFrameRateLimit(60);
    HE3D::Renderer renderer(window, desc.width, desc.height);

    HE3D::Camera camera;
    camera.position = {0, 0, 0};
    camera.fov = 70.0f;

    HE3D::float3 vertices[3] = {
        {-0.8f, -0.6f, 0.0f},
        { 0.0f,  0.8f, 0.0f},
        { 0.8f, -0.6f, 0.0f}
    };
    HE3D::float2 uvs[3] = {{0,0}, {0.5f,1}, {1,0}};

    HE3D::GameObject object;
    object.mesh = HE3D::Mesh::Create(vertices, uvs, 3);
    object.position = {0, 0, 3};
    if (!object.mesh) {
        HE3D::DestroyWindow(window);
        return 1;
    }

    while (!quit && !HE3D::WindowShouldClose(window)) {
        double frameStart = HE3D::TimeSeconds();
        HE3D::PollEvents(window);
        renderer.Clear({0.08f, 0.10f, 0.14f});
        renderer.DrawGameObject(object, camera, HE3D::color3(1.0f, 0.35f, 0.15f));
        renderer.Present();
        HE3D::PaceFrame(frameStart);
    }

    delete object.mesh;
    HE3D::DestroyWindow(window);
    return 0;
}
```

| Resource | Created by | Released by |
| --- | --- | --- |
| `Window` | `CreateWindow()` | `DestroyWindow(window)` |
| file data | `LoadFile()` | `CloseFile(&file)` |
| `Mesh` | `Mesh::Create()`, `Mesh::LoadOBJ()` | `delete mesh` |
| `Texture` | `Texture::LoadBMP()`, `LoadPNG()`, `LoadJPG()`, `LoadImage()` | `delete texture` |
| `Renderer` | constructor | scope exit or `delete renderer` |

`GameObject`, `Camera`, and `CollisionBox` do not own meshes or textures.

## 3. Platform API

Declared in `include/he3d_platform.hpp`.

### 3.1 Types

| Type | Fields | Meaning |
| --- | --- | --- |
| `int8_t`...`int64_t` | none | HE3D fixed-width signed integers. |
| `uint8_t`...`uint64_t` | none | HE3D fixed-width unsigned integers. |
| `ColorA` | `uint8_t r, g, b, a` | 8-bit RGBA pixel; fixed 4-byte layout. |
| `FileData` | `handle, data, length` | File content returned by `LoadFile()`. |
| `Window` | opaque | Backend window handle. |
| `WindowDesc` | `width, height, title, flags` | Window creation parameters. Width/height must be greater than 0. |
| `KeyCallback` | `void (*)(int32_t key, bool pressed, void *user)` | Keyboard event callback. |

### 3.2 Platform Table

`Platform` is implemented by backends. Normal applications do not fill this table.

| Field | Purpose |
| --- | --- |
| `alloc/free` | Memory allocator used by HE3D and global `new/delete`. |
| `loadFile/closeFile` | Read and release resource files. |
| `createWindow/destroyWindow` | Window lifetime. |
| `setWindowTitle` | Window title updates. Unsupported backends may ignore it. |
| `setKeyCallback/pollEvents/shouldClose` | Input and close events. |
| `timeSeconds/sleepMilliseconds` | Monotonic time and millisecond sleep. |
| `present` | Present RGBA framebuffer pixels. |

### 3.3 Platform Functions

| Function | Parameters | Return value | Contract |
| --- | --- | --- | --- |
| `GetPlatform()` | none | `const Platform *` | Active platform table; falls back to built-in backend. |
| `SetPlatform(platform)` | table pointer or `nullptr` | none | Must be called before any HE3D allocation/window/file operation when using a custom backend. |
| `GetBuiltinPlatform()` | none | `const Platform *` | Built-in backend table for the current build. |
| `Alloc(size)` | bytes | pointer or `nullptr` | Allocates through the active platform. |
| `Free(ptr)` | pointer | none | Releases platform memory; `nullptr` must be safe. |
| `LoadFile(path, outFile)` | path and output struct | `bool` | On success, `data` remains valid until `CloseFile()`. |
| `CloseFile(file)` | file struct | none | Releases file data and clears the struct. |
| `CreateWindow(desc)` | window description | `Window *` or `nullptr` | Creates a window. |
| `SetWindowTitle(window, title)` | window and title | none | Updates the title. |
| `DestroyWindow(window)` | window | none | Destroys a window. |
| `SetKeyCallback(window, callback, user)` | callback and user pointer | none | Registers input callback; `nullptr` cancels it. |
| `PollEvents(window)` | window | none | Must be called every frame; callbacks are delivered here. |
| `WindowShouldClose(window)` | window | `bool` | Reports close request. |
| `TimeSeconds()` | none | `double` | Monotonic seconds with subsecond precision. |
| `SleepMilliseconds(milliseconds)` | milliseconds | none | Sleeps for the requested millisecond duration. |
| `Present(window, width, height, pixels)` | RGBA framebuffer | none | Backend framebuffer submission. Applications usually use `Renderer::Present()`. |

Keyboard contract:

- `pressed == true` means key down; `false` means key up.
- `Esc` is ASCII `27`.
- Examples normalize `'A'..'Z'` to lowercase before updating key state.
- Backends with real key-up must forward real down/up events.
- Backends should not filter repeat key-down events with an extra state table.

Time contract:

- `TimeSeconds()` is seconds, not milliseconds or integer ticks.
- It must be monotonic and preserve subsecond precision.
- Compute `deltaTime` from two `TimeSeconds()` calls.
- `PaceFrame(frameStart)` requires `frameStart` from the same clock.

### 3.4 Frame Pacing and Anti-Aliasing State

| Function | Parameters | Return value | Use |
| --- | --- | --- | --- |
| `SetFrameRateLimit(fps)` | FPS; `0` disables | none | Sets global frame pacing. |
| `GetFrameRateLimit()` | none | current limit | `0` means unlimited. |
| `PaceFrame(frameStart)` | frame start seconds | none | Waits for the remaining frame time. |
| `SetFxaaEnabled(enabled)` / `IsFxaaEnabled()` | switch / none | none / switch | FXAA toggle read by `Renderer::Present()`. |
| `SetTaaEnabled(enabled)` / `IsTaaEnabled()` | switch / none | none / switch | TAA toggle read by `Renderer::Present()`. Disabling clears history. |
| `SetMsaaEnabled(enabled)` / `IsMsaaEnabled()` | switch / none | none / switch | MSAA toggle read while rasterizing later triangles. |
| `SetSsaaScale(scale)` / `GetSsaaScale()` | scale / none | none / scale | Global SSAA scale clamped to `1..4`; copied by renderer construction or `Resize()`. |

After changing SSAA at runtime:

```cpp
HE3D::SetSsaaScale(2);
renderer.Resize(width, height);
```

## 4. Math API

Declared in `include/he3d_math.h`. It does not depend on libm or platform APIs.

### 4.1 Macros and Constants

| Name | Use |
| --- | --- |
| `HE3D_MIN`, `HE3D_MAX`, `HE3D_CLAMP`, `HE3D_ABS` | Basic value helpers. |
| `HE3D_DEG2RAD(d)` | Degrees to radians. |
| `HE3D_PI`, `HE3D_TAU`, `HE3D_PI_DIV_2` | Precomputed constants. |

### 4.2 Scalar Functions

| Function | Return value | Notes |
| --- | --- | --- |
| `fabsf`, `abs` | absolute value | Float and `int32_t` versions. |
| `floorf`, `ceilf`, `roundf`, `fracf` | rounded/fractional value | `fracf` maps negative fractions into `[0,1)`. |
| `sqrtf_precise`, `sqrtf`, `rsqrtf` | square root / reciprocal square root | Inputs `<= 0` return `0`. |
| `sinf`, `cosf`, `tanf`, `sincosf`, `atan2f` | trigonometric values | Inputs and outputs are radians where applicable. |

### 4.3 Vectors, Color, Matrix

| Type | Fields | Operations |
| --- | --- | --- |
| `float2` | `x, y` | `+`, `-`, scalar `* /`, component `*`. |
| `float3` | `x, y, z` | `+`, `-`, unary `-`, scalar `* /`, component `*`. |
| `color3` | `r, g, b` | `+`, `-`, scalar `* /`, component `*`. |
| `float4x4` | `m[16]` | Default identity matrix storage. |

`float3` helpers:

| Function | Return value | Use |
| --- | --- | --- |
| `lengthSq()`, `length()` | `float` | Vector length. |
| `normalize()`, `normalizeFast()` | `float3` | Unit vector; near-zero input returns `{0,0,0}`. |
| `float3::dot(a,b)`, `cross(a,b)`, `lerp(a,b,t)` | scalar/vector | Common vector operations. |
| `rotateX/Y/Z(angle)` | `float3` | Axis rotation in radians. |
| `rotateX/Y/Z(s,c)` | `float3` | Axis rotation with caller-provided sin/cos. |

### 4.4 Rays and AABB

| Type | Fields | Default |
| --- | --- | --- |
| `RayHit` | `hit, distance, position, normal, u, v` | `hit=false`, values zero. |
| `AABB` | `min, max` | zero min/max. |
| `Ray` | `origin, direction` | origin zero, direction +Z. |

| Function | Return value | Use |
| --- | --- | --- |
| `AABB::Intersects(other)` | `bool` | AABB overlap test. |
| `Ray::FromTo(from,to)` | `Ray` | Direction is normalized from `from` to `to`. |
| `Ray::Normalized()` | `Ray` | Copy with normalized direction. |
| `Ray::At(distance)` | `float3` | Position along ray. |
| `IntersectSphere`, `IntersectPlane`, `IntersectTriangle`, `IntersectAABB` | `bool` | Intersection tests with optional output distances. |
| `CastSphere`, `CastPlane`, `CastTriangle` | `RayHit` | Full hit record; default record on miss. |

Use a unit `Ray::direction` when distances must be world distances.

### 4.5 Quaternion

| Function | Return value | Use |
| --- | --- | --- |
| `quat(w=1,x=0,y=0,z=0)` | `quat` | Identity by default. |
| `quat::FromEuler(euler)` / `FromEulerFast(euler)` | `quat` | Radians; `x=pitch`, `y=yaw`, `z=roll`. |
| `normalize()` / `normalizeFast()` | `quat` | Normalized rotation; near-zero returns identity. |
| `operator*(q)` | `quat` | Compose rotations. |
| `rotate(v)` | `float3` | Rotate vector. |
| `inverse()` | `quat` | Conjugate, valid as inverse for unit quaternions. |

## 5. Rendering Resources and Scene Objects

Declared in `include/he3d.hpp`.

### 5.1 `DirectionalLight`

| Field | Default | Meaning |
| --- | --- | --- |
| `direction` | `{0,-1,1}` | Light ray direction, not a position. |
| `color` | `{1,1,1}` | Light color and strength. |
| `ambient` | `0.15f` | Base ambient brightness. |

Set it with `renderer.SetMainLight(light)`.

### 5.2 `Mesh`

| Field | Meaning |
| --- | --- |
| `vertices` | Vertex array; every 3 vertices form one triangle. |
| `uvs` | UV array with the same capacity as vertices. |
| `triNormals` | One normal per triangle. |
| `vertCount` | Active vertex count. |
| `capacity` | Allocated vertex capacity. |

| Function | Return value | Use |
| --- | --- | --- |
| `Mesh()` | object | Creates an empty mesh. |
| `~Mesh()` | none | Releases `vertices`, `uvs`, and `triNormals`. |
| `Init(vertexCount)` | `bool` | Allocates arrays; `vertexCount <= 0` fails. |
| `Init(srcVertices, srcUvs, vertexCount)` | `bool` | Allocates and copies data; null UVs become zero. |
| `RecalculateTriangleNormals()` | none | Recomputes one normal for every 3 vertices. |
| `Mesh::Create(...)` | `Mesh *` | Allocates and initializes; `nullptr` on failure. |
| `Mesh::LoadOBJ(filename)` | `Mesh *` | Loads OBJ; `nullptr` on failure. |

OBJ support: `v`, `vt`, `f`, fan triangulation, `v/vt`, `v//vn`, `v/vt/vn`. The V component of `vt` is converted to `1 - rawV` for HE3D texture-space coordinates. Negative indices are not supported, faces are limited to 32 vertices, and normals are recomputed.

### 5.3 `Texture`

| Field | Meaning |
| --- | --- |
| `width`, `height` | Pixel dimensions. |
| `pixels` | RGBA8 `ColorA` pixel array. |
| `valid` | True after successful load. |

| Function | Return value | Use |
| --- | --- | --- |
| `Texture()` | object | Creates an empty texture. |
| `~Texture()` | none | Releases `pixels`. |
| `Sample(u,v)` | `color3` | Wrapped nearest-neighbor sample; invalid texture returns magenta. |
| `LoadBMP`, `LoadPNG`, `LoadJPG`, `LoadImage` | `Texture *` | Load texture; caller deletes it; `nullptr` on failure. |

BMP supports uncompressed 24-bit BGR and 32-bit BGRA with top-down or bottom-up rows. PNG/JPG decode to RGBA8. Maximum side length is 8192 pixels.

### 5.4 `GameObject` and `Camera`

| Type | Fields | Meaning |
| --- | --- | --- |
| `GameObject` | `mesh, position, orientation` | Mesh instance. Defaults to null mesh, zero position, identity rotation. Does not own `mesh`. |
| `Camera` | `position, orientation, fov` | Defaults to position `{0,0,5}`, local +Z view direction, and vertical FOV 90 degrees. |

`GameObject::Forward()` returns local +Z rotated into world space.

### 5.5 `Renderer`

```cpp
HE3D::Renderer renderer(window, width, height);
```

`window` must be valid. `width` and `height` must be greater than 0.

| Function | Return value | Use |
| --- | --- | --- |
| `Renderer(window, w, h)` | object | Creates renderer and allocates buffers. |
| `~Renderer()` | none | Releases internal buffers. |
| `Clear(color)` | none | Starts a frame and clears color/depth buffers. |
| `DrawGameObject(obj, cam, color)` | none | Draw object with solid color. |
| `DrawGameObject(obj, cam, texture)` | none | Draw object with texture. |
| `Present()` | none | Resolves FXAA/TAA/SSAA and submits the window. |
| `Resize(w,h)` | none | Rebuilds buffers after size or SSAA changes. |
| `SetMainLight(light)` | none | Sets directional light. |

Frame order:

```cpp
HE3D::PollEvents(window);
renderer.Clear({0.1f, 0.1f, 0.12f});
renderer.DrawGameObject(object, camera, textureOrColor);
renderer.Present();
```

## 6. Anti-Aliasing

HE3D anti-aliasing is controlled only by global functions.

```cpp
HE3D::SetFxaaEnabled(true);
HE3D::SetTaaEnabled(false);
HE3D::SetMsaaEnabled(false);
HE3D::SetSsaaScale(1);
HE3D::Renderer renderer(window, width, height);
```

Runtime changes:

```cpp
HE3D::SetFxaaEnabled(!HE3D::IsFxaaEnabled());
HE3D::SetTaaEnabled(!HE3D::IsTaaEnabled());
HE3D::SetMsaaEnabled(!HE3D::IsMsaaEnabled());

HE3D::SetSsaaScale(2);
renderer.Resize(width, height);
```

| Feature | Functions | Takes effect |
| --- | --- | --- |
| FXAA | `SetFxaaEnabled()` / `IsFxaaEnabled()` | Read by `Renderer::Present()`. |
| TAA | `SetTaaEnabled()` / `IsTaaEnabled()` | Read by `Renderer::Present()`; disabling clears history. |
| MSAA | `SetMsaaEnabled()` / `IsMsaaEnabled()` | Read while rasterizing later triangles. |
| SSAA | `SetSsaaScale()` / `GetSsaaScale()` | Copied by renderer construction or `Renderer::Resize()`. |

Order: MSAA during `DrawGameObject()`, then FXAA, TAA, and SSAA in `Present()`.

Cost notes: FXAA is cheap and can blur edges; TAA can ghost during motion; current MSAA is coverage-only with one depth value; SSAA 2x is about 4x pixels and 4x is about 16x pixels.

## 7. Collision and Physics

### 7.1 `CollisionBox`

| Field | Meaning |
| --- | --- |
| `object` | Bound object; not owned. |
| `centerOffset` | Local center offset. |
| `halfExtents` | Half size; all components must be greater than 0. |

| Function | Return value | Use |
| --- | --- | --- |
| `CollisionBox()` / `CollisionBox(gameObject)` | object | Creates an empty or bound box. |
| `BindGameObject()` | none | Rebinds object. |
| `FitMesh()` / `FitMesh(mesh)` | `bool` | Fits local AABB from mesh. |
| `Center()`, `Orientation()`, `IsValid()` | value | Query box state. |
| `Contains(point)`, `Intersects(other)` | `bool` | OBB tests. |
| `Contact(other, outContact)` | `bool` | SAT contact query. |
| `Raycast(ray)` | `RayHit` | Ray query. |
| `WorldAABB()` | `AABB` | AABB enclosing the OBB. |

### 7.2 Contacts and Materials

| Type | Fields | Meaning |
| --- | --- | --- |
| `PhysicsContact` | `hit, penetration, normal, point` | Contact result. |
| `PhysicsMaterial` | `restitution, friction, linearDamping, angularDamping, drag` | Surface response parameters. |

### 7.3 `HeightFieldCollider`

```cpp
static float TerrainHeight(float x, float z, void *user) { return ...; }
HE3D::HeightFieldCollider collider(TerrainHeight, user, gridCount, cellSize);
```

`HeightFieldCollider::Tile` stores cached cell AABBs for one terrain tile.

| `Tile` field or function | Meaning |
| --- | --- |
| `active` | Whether this cache is usable. |
| `originX`, `originZ` | World-space tile origin. |
| `bounds` | AABB for the whole tile. |
| `cellBounds` | Per-cell AABB cache array. |
| `cellCount`, `cellCapacity` | Active cell count and capacity. |
| `Tile()` / `~Tile()` | Creates an empty tile and releases `cellBounds`. |
| `EnsureCapacity(count)` | Ensures the cache can store at least `count` cells. |

| Function | Return value | Use |
| --- | --- | --- |
| `HeightFieldCollider()` | object | Creates an invalid heightfield. |
| `HeightFieldCollider(callback,user,gridCount,cellSize)` | object | Creates and configures a heightfield. |
| `Configure()` | none | Sets callback, user pointer, grid count, and cell size. |
| `IsValid()` | `bool` | Callback exists, `gridCount >= 2`, `cellSize > 0`. |
| `BuildTile(tile, originX, originZ)` | `bool` | Builds cached cell AABBs. |
| `ContactBox(box, sweep, outContact)` | `bool` | Direct heightfield contact query. |
| `ContactBox(box, tiles, tileCount, sweep, outContact)` | `bool` | Cached-tile contact query. |
| `NormalAt(x,z)` | `float3` | Estimated terrain normal. |

`sweep > 0` expands the query AABB to reduce tunneling.

### 7.4 `CollisionBoxSet` and Raycasts

| Function | Return value | Use |
| --- | --- | --- |
| `CollisionBoxSet()` / `CollisionBoxSet(gameObject)` | object | Creates an empty or bound set. |
| `~CollisionBoxSet()` | none | Releases the internal box array. |
| `FitMeshGrid(xParts,yParts,zParts)` | `bool` | Splits the bound mesh into fitted boxes. |
| `FitMeshGrid(mesh,xParts,yParts,zParts)` | `bool` | Splits a specified mesh. |
| `IsValid()`, `Contains()`, `Intersects()`, `Raycast()` | value | Queries over the box set. |
| `RaycastCollisionBox()` | `bool` | Single box raycast. |
| `RaycastCollisionBoxes()` | `bool` | Array raycast with optional index. |
| `RaycastCollisionBoxSet()` | `bool` | Box set raycast with optional index. |
| `RaycastMeshTriangles()` | `bool` | Mesh triangle raycast with object transform. |

### 7.5 `PhysicsBody`

| Field | Meaning |
| --- | --- |
| `object` | Bound object; not owned. |
| `velocity`, `angularVelocity` | Linear and angular velocity. |
| `force`, `torque` | Accumulated force/torque for next integration. |
| `gravity` | Gravity acceleration. |
| `mass/inverseMass`, `inertia/inverseInertia` | Mass/inertia and reciprocals. |
| `linearDamping`, `angularDamping`, `drag` | Damping and drag. |
| `restitution`, `friction`, `material` | Collision response settings. |

| Function | Return value | Use |
| --- | --- | --- |
| `PhysicsBody()` / `PhysicsBody(gameObject)` | object | Creates an unbound or bound body. |
| `BindGameObject()` | none | Rebinds object. |
| `SetEnabled()` / `IsEnabled()` | none / `bool` | Enables integration. |
| `SetMass()` / `SetInertia()` | none | Values `<= 0` make inverse value 0. |
| `SetMaterial()` | none | Copies material and synchronizes response fields. |
| `SetVelocity()`, `SetAngularVelocity()` | none | Set velocities. |
| `AddForce()`, `AddTorque()` | none | Accumulate for next `Step()`. |
| `AddImpulse()`, `AddAngularImpulse()` | none | Immediately modifies velocity. |
| `ClearForces()`, `ClearTorques()` | none | Clear accumulated force/torque. |
| `Step(deltaTime)` | none | Integrates bound object and clears force/torque. |
| `StepWithCollisions(...)` | `bool` | Substep integration with box obstacles. |
| `ResolveHeightField(...)` | `bool` | Resolves terrain contact. |

`Step()` returns immediately when disabled, unbound, or `deltaTime <= 0`.

## 8. Built-In Backends

| Backend | Behavior |
| --- | --- |
| XAPI | Creates an XJ380 GUI window and presents `ColorA` buffers through `xapi_WriteBufferA`; keyboard callbacks are delivered from `PollEvents()`. |
| SDL3 | Creates an SDL window, renderer, and RGBA32 streaming texture; forwards key down/up and releases tracked keys on focus loss. |
| CONSOLE | Displays the framebuffer with ANSI 24-bit color; synthesizes key release when repeated terminal input stops. |

Backend requirements:

- `alloc/free` must serve all HE3D objects; `free(nullptr)` is safe.
- `loadFile` returns complete file contents and leaves no ownership on failure.
- `timeSeconds` returns monotonic seconds with subsecond precision.
- `sleepMilliseconds` must honor millisecond input.
- `present` receives memory-order RGBA pixels.

## 9. Examples

| Example | Directory | Purpose | Controls |
| --- | --- | --- | --- |
| `TriangleTest` | `examples/TriangleTest/` | Minimal window, callback, mesh, renderer, loop, cleanup. | `Esc` quits. |
| `AirplaneTest` | `examples/AirplaneTest/` | Large OBJ renderer and AA stress test. | `T` TAA, `M` MSAA, `1..4` SSAA, `Esc` quits. |
| `PhysicsTest` | `examples/PhysicsTest/` | Physics sample with gravity, collision boxes, ground contact, force, torque, and impulse. | `A/D` horizontal force, `Space` jump, `R` reset, `F` FXAA, `Esc` quits. |
| `FlightSimulator` | `examples/FlightSimulator/` | Flight sample with input, camera, OBJ/BMP, terrain tiles, heightfield contact, and physics controls. | `W/S` pitch, `Q/E` yaw, `A/D` roll, `F` FXAA, `Esc` quits. |

FlightSimulator rules:

- Input comes only from the `SetKeyCallback()` state table.
- Letters are normalized to lowercase in the callback.
- `InputCurve()` smooths key input before incremental quaternion rotation.
- Forward speed is `plane.Forward() * 15.0f`.
- Render terrain tiles and `HeightFieldCollider::Tile` caches are generated together.
- After physics integration, `ResolveHeightField()` uses a velocity-based sweep to reduce tunneling.
