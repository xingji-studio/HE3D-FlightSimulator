# HE3D 新手入门

[英文版](getting-started.en.md)

这份文档只讲最常用的工作流：怎么编译、怎么选示例、怎么新增示例、怎么把自己的代码链接到 HE3D。

## 两个 CMake 变量

HE3D 现在用两个变量决定构建内容：

| 变量 | 作用 | 可选值 |
| --- | --- | --- |
| `HE3D_BACKEND` | 选择运行平台 | `SDL3`、`CONSOLE`、`XAPI` |
| `HE3D_EXAMPLE` | 选择示例程序 | `TriangleTest`、`AirplaneTest`、`PhysicsTest`、`FlightSimulator` |

例子：

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

`build/` 可以一直用同一个。切换变量后重新运行 `cmake -S . -B build ...`，再 `cmake --build build`。

## 第一个示例

`examples/TriangleTest/src/main.cpp` 是最小示例。它只做这些事：

1. `HE3D::CreateWindow()` 创建窗口。
2. `HE3D::SetKeyCallback()` 注册键盘回调。
3. `HE3D::Renderer` 创建软件渲染器。
4. `HE3D::Mesh::CreateTriangle()` 创建一个三角形。
5. 主循环里调用 `PollEvents()`、`Clear()`、`DrawGameObject()`、`Present()`。
6. 程序结束时 `delete mesh`，再 `DestroyWindow()`。

如果你刚开始写 HE3D 程序，先复制 `examples/TriangleTest`，不要直接从飞行模拟器开始改。`AirplaneTest` 演示大型 OBJ 资源加载，`PhysicsTest` 演示重力、碰撞和冲量，飞行模拟器包含地形、物理、输入曲线、相机跟随和资源加载；它们适合当专项或完整项目参考，不适合当第一份代码。

## 源码阅读顺序

如果只会基本 C++，建议按这个顺序看源码：

1. `examples/TriangleTest/src/main.cpp`：先看 HE3D 程序最小结构。
2. `include/he3d.hpp`：看公开类型，不需要先看实现。
3. `examples/AirplaneTest/src/main.cpp`：看 OBJ 文件怎么作为运行时资源加载。
4. `examples/PhysicsTest/src/main.cpp`：看物理体、碰撞箱、力和冲量怎么通过公开 API 组合。
5. `examples/FlightSimulator/src/main.cpp`：再看完整示例怎么组织输入、相机、资源和主循环。
6. `src/he3d.cpp`：最后再看渲染器、OBJ/图片加载、碰撞和物理实现。
7. `include/he3d_platform.hpp` 和 `src/platform/`：只有在你要写新后端时再看。

几个核心名词：

| 名词 | 直白解释 |
| --- | --- |
| `Mesh` | 一堆三角形顶点，是“形状”。 |
| `GameObject` | 一个放在世界里的对象，保存 `mesh`、位置和旋转。 |
| `Camera` | 观察者，从哪里看、朝哪里看、视野多大。 |
| `Renderer` | 负责清屏、绘制物体、显示画面。 |

## 新增自己的示例

假设要新增 `examples/MyDemo`：

1. 建目录：

```text
examples/MyDemo/src/main.cpp
examples/MyDemo/assets/
```

2. 在 `examples/MyDemo/src/main.cpp` 里写自己的 `main()`。

3. 打开 `CMakeLists.txt`，添加源码变量：

```cmake
set(HE3D_MYDEMO_SOURCES
    examples/MyDemo/src/main.cpp
)
```

4. 把 `HE3D_EXAMPLE` 的可选值加入 `MyDemo`，再在选择示例的 `if()` 里添加：

```cmake
elseif(HE3D_EXAMPLE STREQUAL "MyDemo")
    set(HE3D_APP_SOURCES ${HE3D_MYDEMO_SOURCES})
    set(HE3D_APP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/MyDemo/src)
    set(HE3D_APP_NAME he3d_mydemo)
endif()
```

5. 构建：

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=MyDemo
cmake --build build
./build/he3d_mydemo_sdl3
```

## 链接自己的代码

HE3D 的构建分两层：

- `he3d_engine_sdl3`、`he3d_engine_console`、`he3d_engine_xapi` 是可链接的 HE3D 库。
- `he3d_sdl3_app`、`he3d_console_app`、`he3d_xapi_app` 是示例程序。

自己的程序只要链接对应后端的引擎库即可。以 SDL3 为例：

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

你的 C++ 文件里包含：

```cpp
#include "he3d.hpp"
```

然后使用 `HE3D::Window`、`HE3D::Renderer`、`HE3D::Mesh`、`HE3D::Camera` 等类型。

## 一个 HE3D 程序的固定顺序

大多数 HE3D 程序都按这个顺序写：

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

常用基础形状：

| API | 形状 | 说明 |
| --- | --- | --- |
| `HE3D::Mesh::CreateTriangle(width, height)` | 三角形 | XY 平面，中心在原点。 |
| `HE3D::Mesh::CreatePlane(width, depth)` | 平面 | XZ 平面，中心在原点。 |
| `HE3D::Mesh::CreateCube(width, height, depth)` | 盒子 | 立方体/长方体，中心在原点。 |
| `HE3D::Mesh::CreateSphere(radius, segments, rings)` | 球体 | `segments` 最小为 3，`rings` 最小为 2。 |

最容易出错的地方：

- 忘记每帧调用 `PollEvents()`，键盘和关闭窗口就不会更新。
- 物体在相机后面。默认相机朝 +Z 看，所以新手示例把物体放在 `z = 3`。
- 三角形顶点顺序反了。HE3D 会做背面剔除，看不到时先交换第二、第三个顶点。
- 忘记释放 `Mesh::Create*()`、`Mesh::LoadOBJ()` 或 `Texture::Load*()` 返回的对象。
