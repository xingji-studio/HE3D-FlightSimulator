# HE3D

[English](README.md)

HE3D 是一个带轻量平台层的 C++11 软件渲染 3D 引擎。同一个飞行模拟器示例可以构建为 XJ380/XAPI 程序，也可以构建为原生 SDL3 程序。

## 功能

- 带 Z-buffer 深度测试的软件三角形光栅化
- 支持加载双翼机 OBJ 网格
- 支持加载 24-bit 未压缩 BMP 纹理
- 支持纯色和纹理网格的方向光照
- 基于四元数的飞行姿态控制
- 平滑追踪相机
- 可回收的程序化地形 tile
- 当 `biplane.obj` 无法加载时，使用程序化备用飞机网格
- 类似 LVGL 的易移植平台接口，覆盖内存、文件、窗口、输入、计时和 framebuffer 提交

## 控制

- `W` / `S`：俯仰
- `Q` / `E`：偏航
- `A` / `D`：滚转
- `Esc`：退出

## 项目结构

- `include/`：HE3D 引擎公开头文件
- `include/he3d.hpp`：引擎类接口
- `include/he3d_platform.hpp`：LVGL 风格的平台接口
- `include/he3d_math.h`：紧凑数学库，包含向量、四元数和基础标量数学
- `src/he3d.cpp`：网格初始化、OBJ/BMP 加载、渲染器和光栅化器
- `src/platform/he3d_platform_xapi.cpp`：XJ380/XAPI 后端
- `src/platform/he3d_platform_sdl3.cpp`：SDL3 后端
- `examples/FlightSimulator/`：飞行模拟器示例
- `examples/FlightSimulator/src/`：飞行模拟入口和地形网格生成
- `examples/FlightSimulator/assets/`：飞机网格和纹理资源，构建后会复制到可执行文件目录
- `xj380/include/`：XJ380 API 与兼容头文件

## 构建

项目使用 CMake 和 Ninja。Release 使用 GCC 和 `-O2`，Debug 使用 Clang 并开启调试符号。

```sh
cmake -S . -B build/release-xapi -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DHE3D_BACKEND=XAPI
cmake --build build/release-xapi

cmake -S . -B build/release-sdl3 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DHE3D_BACKEND=SDL3
cmake --build build/release-sdl3

cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DHE3D_BACKEND=XAPI
cmake --build build/debug
```

用 `-DHE3D_BACKEND=XAPI` 或 `-DHE3D_BACKEND=SDL3` 选择一个后端。默认是 `XAPI`。

`XAPI` 目标：

- `he3d_engine_xapi`：引擎核心加 XAPI 后端
- `he3d_flight_simulator.elf`：XJ380/XAPI 飞行模拟器

`SDL3` 目标：

- `he3d_engine_sdl3`：引擎核心加 SDL3 后端
- `he3d_flight_simulator_sdl3`：原生 SDL3 飞行模拟器

## 清理

```sh
cmake --build build/release-xapi --target clean
cmake --build build/release-sdl3 --target clean
cmake --build build/debug --target clean
```
