# HE3D Getting Started

[Chinese version](getting-started.md)

This document covers the common workflow: building, choosing an example, adding a new example, and linking your own code to HE3D.

## Two CMake Variables

HE3D uses two variables to decide what to build:

| Variable | Purpose | Values |
| --- | --- | --- |
| `HE3D_BACKEND` | Selects the platform backend | `SDL3`, `CONSOLE`, `XAPI` |
| `HE3D_EXAMPLE` | Selects the example program | `TriangleTest`, `AirplaneTest`, `PhysicsTest`, `FlightSimulator` |

Examples:

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=TriangleTest
cmake --build build
./build/he3d_triangle_test_sdl3
```

```sh
cmake -S . -B build -DHE3D_BACKEND=CONSOLE -DHE3D_EXAMPLE=TriangleTest
cmake --build build
./build/he3d_triangle_test_console
```

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=FlightSimulator
cmake --build build
./build/he3d_flight_simulator_sdl3
```

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=AirplaneTest
cmake --build build
./build/he3d_airplane_test_sdl3
```

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=PhysicsTest
cmake --build build
./build/he3d_physics_test_sdl3
```

You can keep using the same `build/` directory. Re-run `cmake -S . -B build ...` after changing variables, then run `cmake --build build`.

## First Example

`examples/TriangleTest/src/main.cpp` is the smallest example. It does this:

1. Creates a window with `HE3D::CreateWindow()`.
2. Registers keyboard input with `HE3D::SetKeyCallback()`.
3. Creates a software renderer with `HE3D::Renderer`.
4. Creates one triangle with `HE3D::Mesh::CreateTriangle()`.
5. Calls `PollEvents()`, `Clear()`, `DrawGameObject()`, and `Present()` every frame.
6. Deletes the mesh and destroys the window before exit.

Start by copying `examples/TriangleTest`. `AirplaneTest` demonstrates loading a large OBJ runtime asset, `PhysicsTest` demonstrates gravity, collision, and impulses, and the flight simulator is a full project example with terrain, physics, input curves, camera follow, and asset loading. They are better as focused or full-project references than as first code to modify.

## Source Reading Order

If you only know basic C++, read the source in this order:

1. `examples/TriangleTest/src/main.cpp`: learn the smallest HE3D program shape.
2. `include/he3d.hpp`: read the public types before reading their implementation.
3. `examples/AirplaneTest/src/main.cpp`: see how an OBJ file is loaded as a runtime asset.
4. `examples/PhysicsTest/src/main.cpp`: see how physics bodies, collision boxes, forces, and impulses are combined through public APIs.
5. `examples/FlightSimulator/src/main.cpp`: see how a full example organizes input, camera, assets, and the main loop.
6. `src/he3d.cpp`: read renderer, OBJ/image loading, collision, and physics implementation last.
7. `include/he3d_platform.hpp` and `src/platform/`: read these only when you want to write a new backend.

Core terms:

| Term | Plain meaning |
| --- | --- |
| `Mesh` | Triangle vertex data, the shape itself. |
| `GameObject` | An object in the world, with a `mesh`, position, and rotation. |
| `Camera` | The viewer: where it is, where it looks, and how wide the view is. |
| `Renderer` | Clears the screen, draws objects, and shows the frame. |

## Add Your Own Example

Assume the new example is `examples/MyDemo`:

1. Create directories:

```text
examples/MyDemo/src/main.cpp
examples/MyDemo/assets/
```

2. Write your own `main()` in `examples/MyDemo/src/main.cpp`.

3. Open `CMakeLists.txt` and add a source variable:

```cmake
set(HE3D_MYDEMO_SOURCES
    examples/MyDemo/src/main.cpp
)
```

4. Add `MyDemo` to the allowed `HE3D_EXAMPLE` values, then add this branch to the example selection block:

```cmake
elseif(HE3D_EXAMPLE STREQUAL "MyDemo")
    set(HE3D_APP_SOURCES ${HE3D_MYDEMO_SOURCES})
    set(HE3D_APP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/MyDemo/src)
    set(HE3D_APP_NAME he3d_mydemo)
endif()
```

5. Build it:

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=MyDemo
cmake --build build
./build/he3d_mydemo_sdl3
```

## Link Your Own Code

HE3D builds in two layers:

- `he3d_engine_sdl3`, `he3d_engine_console`, and `he3d_engine_xapi` are HE3D libraries you can link to.
- `he3d_sdl3_app`, `he3d_console_app`, and `he3d_xapi_app` are example executables.

Your program links to the engine library for the backend you selected. SDL3 example:

```cmake
add_executable(my_game
    examples/MyGame/src/main.cpp
    examples/MyGame/src/game.cpp
)

target_include_directories(my_game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/MyGame/src
)

target_link_libraries(my_game
    PRIVATE
        he3d_engine_sdl3
)
```

In C++:

```cpp
#include "he3d.hpp"
```

Then use `HE3D::Window`, `HE3D::Renderer`, `HE3D::Mesh`, `HE3D::Camera`, and the other public types.

## Normal Program Order

Most HE3D programs follow this order:

```cpp
HE3D::Window *window = HE3D::CreateWindow(&desc);
HE3D::Renderer renderer(window, width, height);

HE3D::Mesh *mesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
HE3D::GameObject object;
object.mesh = mesh;

while (!HE3D::WindowShouldClose(window)) {
    HE3D::PollEvents(window);
    renderer.Clear({0.1f, 0.1f, 0.12f});
    renderer.DrawGameObject(object, camera, HE3D::color3(1, 0, 0));
    renderer.Present();
}

delete mesh;
HE3D::DestroyWindow(window);
```

Common primitive meshes:

| API | Shape | Notes |
| --- | --- | --- |
| `HE3D::Mesh::CreateTriangle(width, height)` | Triangle | XY plane, centered on the origin. |
| `HE3D::Mesh::CreatePlane(width, depth)` | Plane | XZ plane, centered on the origin. |
| `HE3D::Mesh::CreateCube(width, height, depth)` | Box | Cube or rectangular box, centered on the origin. |
| `HE3D::Mesh::CreateSphere(radius, segments, rings)` | Sphere | `segments` clamps to at least 3; `rings` clamps to at least 2. |

Common mistakes:

- Forgetting `PollEvents()` each frame, so keyboard and window-close state never update.
- Placing the object behind the camera. The default camera looks along +Z, so `TriangleTest` places the triangle at `z = 3`.
- Reversing triangle vertex order. HE3D does back-face culling; swap the second and third vertices if a triangle is invisible.
- Forgetting to delete objects returned by `Mesh::Create*()`, `Mesh::LoadOBJ()`, or `Texture::Load*()`.
