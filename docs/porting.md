# HE3D 移植指南

[English version](porting.en.md)

本文说明如何把 HE3D 移植到新平台。移植的核心不是改 renderer，而是实现一个 platform backend。

## Backend 职责

新后端需要实现 `HE3D::Platform`：

| 能力 | 说明 |
| --- | --- |
| 内存 | `alloc` / `free`，供 HE3D 的全局 `new/delete` 使用。 |
| 文件 | `loadFile` / `closeFile`，供 OBJ、BMP、PNG、JPG 加载使用。 |
| 窗口 | 创建、销毁和设置标题；裸平台可以把 Window 当成 framebuffer 句柄。 |
| 输入 | `setKeyCallback` 和 `pollEvents` 交付 key-down/key-up。 |
| 时间 | `timeSeconds` 返回单调秒数；`sleepMilliseconds` 用于帧率限制。 |
| 显示 | `present` 把 `ColorA` framebuffer 提交到平台。 |

## 最小移植目标

第一版后端建议只跑 `TriangleTest`：

1. 实现内存、时间、窗口和 `Present()`。
2. 先用内置 mesh，不读 OBJ/图片。
3. 输入只支持 `Esc` 退出或先不支持退出。
4. 分辨率从小开始，比如 `160x120`、`320x240`。

文件加载、纹理、复杂输入和物理示例可以后补。

## Present

`Present()` 是移植关键。HE3D 传入：

```cpp
void present(Window *window, int32_t width, int32_t height, const ColorA *pixels);
```

`pixels` 是 RGBA 8-bit 数组。后端要把它转换成目标 framebuffer 格式，例如：

- SDL texture。
- 终端字符。
- XJ380 `ColorA` buffer。
- UEFI GOP framebuffer。
- TFT 的 RGB565 buffer。

如果渲染尺寸和屏幕尺寸不同，第一版建议居中或最近邻缩放。

## UEFI

UEFI 很适合 HE3D 的软件渲染模型：

- `Alloc/Free` 对接 Boot Services pool。
- `Window` 保存 GOP framebuffer 信息。
- `Present()` 按 GOP pixel format 转换并拷贝。
- `SleepMilliseconds()` 可用 `Stall()`。
- 文件加载可用 Simple File System Protocol。

第一版 UEFI 目标应是内置三角形或 cube，不要直接移植 FlightSimulator。

## Arduino and MCU

Arduino Uno/Nano 这类 AVR 板 RAM 太小，不适合完整 HE3D。ESP32、Teensy、RP2040 可以做裁剪版：

- 低分辨率。
- 关闭 MSAA/TAA/SSAA。
- 避免大 texture 和 OBJ 运行时加载。
- 优先用内置 mesh 或编译期资源。
- 避免高频 `malloc/free`。

## 后端检查清单

- `CreateWindow()` 失败时返回 `nullptr`。
- `Present()` 不能越界写 framebuffer。
- `PollEvents()` 每帧可调用，且不会阻塞主循环。
- 有 key-up 的平台交付真实 key-up；没有 key-up 的平台需要合成 release。
- `TimeSeconds()` 单调递增。
- `LoadFile()` 返回的数据在 `CloseFile()` 前一直有效。
