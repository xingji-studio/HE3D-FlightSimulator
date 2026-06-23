# HE3D

[English](README.md)

HE3D 是一个 C++11 软件 3D 渲染器。当前主要示例是飞行模拟器，可以构建为 XJ380/XAPI 程序，也可以构建为 SDL3 后端的程序。

## 构建

XAPI：

```sh
cmake -S . -B build -DHE3D_BACKEND=XAPI
cmake --build build
```

SDL3：

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3
cmake --build build
```

可执行文件和资源文件会生成到 `build/`。

## 控制

- `W` / `S`：俯仰
- `Q` / `E`：偏航
- `A` / `D`：滚转
- `Esc`：退出

## 目录

- `include/`：引擎公开头文件
- `src/`：渲染器、加载器和平台后端
- `examples/FlightSimulator/`：飞行模拟器示例
- `xj380/include/`：XAPI 构建使用的 XJ380 头文件
- `docs/technical.zh-CN.md`：实现笔记
