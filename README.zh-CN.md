# HE3D 飞行模拟器

[English](README.md)

HE3D Flight Simulator 是一个面向 XJ380/XACT 平台的 C++11 软件渲染 3D 飞行演示。当前 `src/` 实现直接使用 XJ380 窗口和文件 API，不依赖 C++ 标准库运行时，也不依赖 BridgeEngine。

## 功能

- 带 Z-buffer 深度测试的软件三角形光栅化
- 支持加载双翼机 OBJ 网格
- 支持加载 24-bit 未压缩 BMP 纹理
- 支持纯色和纹理网格的方向光照
- 基于四元数的飞行姿态控制
- 平滑追踪相机
- 可回收的程序化地形 tile
- 当 `Biplane.obj` 无法加载时，使用程序化备用飞机网格

## 控制

- `W` / `S`：俯仰
- `Q` / `E`：偏航
- `A` / `D`：滚转
- `Esc`：退出

## 项目结构

- `src/he3d.hpp`：引擎类接口
- `src/he3d.cpp`：通用网格初始化、OBJ/BMP 加载、渲染器和光栅化器
- `src/he3d_math.h`：紧凑数学库，包含向量、四元数和基础标量数学
- `src/flight_sim.hpp` 与 `src/flight_sim.cpp`：飞行模拟数据和地形 tile 网格生成
- `src/main.cpp`：飞行模拟入口和主循环
- `include/`：XJ380 API 与兼容头文件
- `Biplane.obj`：飞机网格资源
- `biplane.bmp`：飞机纹理资源
- `OldSrc/`：旧版 SDL 原型，保留作参考

## 构建

主构建使用 XJ380 XACT `xxcc` 编译器和 C++11：

```sh
make
```

运行 `make` 前请确保 `xxcc` 已在 `PATH` 中。用户需要自行提供或编译 XJ380 的 `obj-gui` 和 `obj-tui` 运行时对象。

## 清理

```sh
make clean
```

## 说明

当前环境中 `xxcc -c` 已知不稳定，因此 Makefile 使用一次性编译并链接的方式。
