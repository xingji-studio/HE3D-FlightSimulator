# 实现笔记

这里记录改 HE3D 时容易踩到的实现细节。

## 后端

平台层接口在 `include/he3d_platform.hpp`。引擎核心不直接调用 SDL 或 XAPI，而是通过平台层处理内存、文件、窗口、输入、时间和 framebuffer 提交。

当前后端：

- `src/platform/he3d_platform_xapi.cpp`：XJ380/XAPI
- `src/platform/he3d_platform_sdl3.cpp`：SDL3

配置时选一个后端：

```sh
cmake -S . -B build -DHE3D_BACKEND=XAPI
cmake -S . -B build -DHE3D_BACKEND=SDL3
```

仓库只使用一个 `build/`。切换后端时直接重新配置这个目录。

## XJ380 输入

GUI 消息参数以 XJ380 API 手册为准。

键盘消息：

- `MSG_CHAR`：`hData = 0`，`lData = UTF-8 字节`
- `MSG_SPCHAR`：`hData = 0`，`lData = 特殊键编号`

鼠标消息使用窗口相对坐标：

- `MSG_MOVE` 和按键消息：`hData = x`，`lData = y`
- `MSG_ROLLER`：`hData = (x << 32) | y`，`lData = 有符号滚轮偏移`
- `MSG_RESIZE`：两个数据字段保留；窗口大小用 `xapi_GetWindowSize` 查询

XAPI 后端现在只处理键盘消息。XJ380 只发按下事件，所以 `XapiPollEvents` 会用短超时合成释放事件。

## framebuffer

`HE3D::ColorA` 的字段顺序是：

```cpp
struct ColorA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};
```

这个布局和 XAPI 的 `XCOLORA` 一致，XAPI 后端可以把渲染缓冲区直接交给 `xapi_WriteBufferA`。SDL3 后端使用 `SDL_PIXELFORMAT_RGBA32`。

## 渲染器

主要实现都在 `src/he3d.cpp`。

- `Renderer::Clear` 清 color/depth buffer
- `DrawGameObject(..., float3 color)` 绘制纯色网格
- `DrawGameObject(..., const Texture&)` 绘制纹理网格
- depth buffer 存 `1 / z`，值越大越靠近相机
- `Mesh::triNormals` 每个三角形存一条法线

近裁剪目前是三角形级别：顶点在近裁剪面后方时跳过整个三角形，不做跨平面裁剪。

## 资源

飞行模拟器使用：

- `examples/FlightSimulator/assets/biplane.obj`
- `examples/FlightSimulator/assets/biplane.bmp`

构建后资源会复制到可执行文件同目录。OBJ 缺失时会使用内置备用飞机网格。

`Texture::LoadBMP` 支持未压缩 24-bit 和 32-bit BMP。

## 地形流式加载

地形是围绕飞机的固定 3x3 tile 池。tile mesh 只分配一次，后续通过 `UpdatePlaneMesh` 原地刷新。

进入新 tile 时，缺失 tile 会进入待处理列表。主循环每帧最多生成或刷新一个 tile，避免在 XJ380 上跨边界时单帧卡住。

渲染时会跳过相机 X/Z 平面距离过远的 tile，避免稳定运行时画太多不可见地形。

相关常量在 `examples/FlightSimulator/src/main.cpp`：

- `RD`：tile 半径，目前是 `1`
- `GS`：网格间距
- `GC`：单个 tile 的网格点数量
- `TS`：tile 世界尺寸

调大 `GC` 或 `RD` 会直接增加光栅化和地形刷新成本。
