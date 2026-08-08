# HE3D

[English README](README.md)

HE3D 是一个 C++11 软件 3D 渲染器，仓库里包含小示例和一个较完整的飞行模拟器示例。同一份引擎代码可以构建为 XJ380/XAPI、SDL3 或控制台后端程序。

第一次写 HE3D 程序先看 [新手入门](docs/getting-started.md)。
完整文档入口看 [文档索引](docs/index.md)。

## 构建

用 `HE3D_BACKEND` 选择后端，用 `HE3D_EXAMPLE` 选择示例。

SDL3 TriangleTest：

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=TriangleTest
cmake --build build
./build/he3d_triangle_test_sdl3
```

SDL3 飞行模拟器：

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=FlightSimulator
cmake --build build
./build/he3d_flight_simulator_sdl3
```

SDL3 AirplaneTest：

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=AirplaneTest
cmake --build build
./build/he3d_airplane_test_sdl3
```

SDL3 PhysicsTest：

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=PhysicsTest
cmake --build build
./build/he3d_physics_test_sdl3
```

控制台：

```sh
cmake -S . -B build -DHE3D_BACKEND=CONSOLE -DHE3D_EXAMPLE=TriangleTest
cmake --build build
./build/he3d_triangle_test_console
```

XAPI：

```sh
cmake -S . -B build -DHE3D_BACKEND=XAPI -DHE3D_EXAMPLE=FlightSimulator
cmake --build build
```

XAPI 构建前需要先准备 XJ380 GUI 运行库对象。CMake 会查找 `../XXCC-suite/obj-gui` 和 `../XJ380/out/xapi`，也可以用 `-DXJ380_GUI_RUNTIME_DIR=/path/to/runtime` 指定。

可执行文件和资源文件会生成到 `build/`。

## 飞行模拟器示例控制

- `W` / `S`：俯仰
- `Q` / `E`：偏航
- `A` / `D`：滚转
- `F`：开关 FXAA
- `Esc`：退出

## 目录

- `include/`：引擎公开头文件
- `src/`：渲染器、加载器和平台后端
- `tests/`：测试目录和边界说明
- `examples/TriangleTest/`：最小示例
- `examples/AirplaneTest/`：OBJ 飞机模型加载示例
- `examples/PhysicsTest/`：物理和碰撞示例
- `examples/FlightSimulator/`：飞行模拟器示例
- `xj380/include/`：XAPI 构建使用的 XJ380 头文件
- [docs/index.md](docs/index.md)：文档索引
- [docs/getting-started.md](docs/getting-started.md)：新手入门
- [docs/concepts.md](docs/concepts.md)：核心概念和模块职责
- [docs/api-guide.md](docs/api-guide.md)：公开 API 使用指南
- [docs/examples.md](docs/examples.md)：示例说明
- [docs/porting.md](docs/porting.md)：后端移植指南
- [docs/technical.md](docs/technical.md)：技术总览、完整 API 手册和维护规则

## 许可证

[MIT](LICENSE)
