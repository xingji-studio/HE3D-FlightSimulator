# HE3D 技术手册

[English version](technical.en.md)

本文档按公开头文件说明 HE3D 的用法和契约。应用代码主要包含：

```cpp
#include "he3d.hpp"
```

`he3d.hpp` 会包含 `he3d_math.h` 和 `he3d_platform.hpp`。除全局 `new` / `delete` 外，公开 C++ 符号都在 `HE3D` 命名空间。

`he3d.hpp` 定义全局 `operator new`、`operator new[]`、`operator delete`、`operator delete[]`，它们转发到当前 HE3D 平台分配器。因此应用应在创建 HE3D 对象前保证平台后端已经可用。

## 1. 构建

HE3D 一次构建一个后端和一个示例：

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3 -DHE3D_EXAMPLE=TriangleTest
cmake --build build
```

CMake 选项：

| 选项 | 取值 | 默认值 | 作用 |
| --- | --- | --- | --- |
| `HE3D_BACKEND` | `XAPI`、`SDL3`、`CONSOLE` | `XAPI` | 选择平台后端。 |
| `HE3D_EXAMPLE` | `TriangleTest`、`AirplaneTest`、`PhysicsTest`、`FlightSimulator` | `TriangleTest` | 选择构建的示例程序。 |
| `XJ380_SDK_ROOT` | 路径 | `../XXCC-suite` | XAPI 构建查找旧版 XJ380 SDK 运行库对象的根目录。 |
| `XJ380_GUI_RUNTIME_DIR` | 路径 | 空 | 显式指定 XJ380 GUI 运行库对象目录。 |

输出文件名：

| 后端 | CMake 目标 | 输出文件 |
| --- | --- | --- |
| XAPI | `he3d_xapi_app` | `build/he3d_<example>.elf` |
| SDL3 | `he3d_sdl3_app` | `build/he3d_<example>_sdl3` |
| CONSOLE | `he3d_console_app` | `build/he3d_<example>_console` |

XAPI 构建会链接预编译 XJ380 GUI 运行库对象。查找顺序是：

1. `-DXJ380_GUI_RUNTIME_DIR=/path/to/runtime`
2. `${XJ380_SDK_ROOT}/obj-gui`
3. `../XJ380/out/xapi`

使用 `../XJ380/out/xapi` 时，构建系统会排除 `constart.cpp.o`，避免控制台启动对象和 GUI 程序入口重复定义符号。

## 2. 最小程序结构

一个 HE3D 程序按下面顺序写：

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

生命周期规则：

| 资源 | 创建函数 | 释放方式 |
| --- | --- | --- |
| `Window` | `CreateWindow()` | `DestroyWindow(window)` |
| 文件数据 | `LoadFile()` | `CloseFile(&file)` |
| `Mesh` | `Mesh::Create()`、`Mesh::LoadOBJ()` | `delete mesh` |
| `Texture` | `Texture::LoadBMP()`、`LoadPNG()`、`LoadJPG()`、`LoadImage()` | `delete texture` |
| `Renderer` | 构造函数 | 离开作用域或 `delete renderer` |

`GameObject`、`Camera`、`CollisionBox` 不拥有 mesh 或 texture。释放对象时不要让 `GameObject::mesh` 指向已经释放的 mesh 后继续绘制。

## 3. 平台 API

平台 API 在 `include/he3d_platform.hpp`。

### 3.1 基础类型

| 类型 | 字段 | 说明 |
| --- | --- | --- |
| `int8_t`、`int16_t`、`int32_t`、`int64_t` | 无 | HE3D 固定宽度有符号整数。 |
| `uint8_t`、`uint16_t`、`uint32_t`、`uint64_t` | 无 | HE3D 固定宽度无符号整数。 |
| `ColorA` | `uint8_t r, g, b, a` | 8-bit RGBA 像素。内存布局固定为 4 字节。 |
| `FileData` | `void *handle; const uint8_t *data; uint64_t length` | `LoadFile()` 返回的文件内容。 |
| `Window` | 不透明 | 后端窗口句柄。应用不能直接访问字段。 |
| `WindowDesc` | `width, height, title, flags` | 创建窗口所需参数。`width/height` 必须大于 0。 |
| `KeyCallback` | `void (*)(int32_t key, bool pressed, void *user)` | 键盘事件回调。 |

### 3.2 平台函数表

`Platform` 是后端函数表。普通应用不需要填写它；只有写新后端时才实现。

| 字段 | 作用 |
| --- | --- |
| `alloc/free` | 给 HE3D 对象和全局 `new/delete` 使用的内存分配器。 |
| `loadFile/closeFile` | 读取并释放资源文件。 |
| `createWindow/destroyWindow` | 创建和销毁窗口。 |
| `setWindowTitle` | 修改窗口标题；不支持标题的后端可以忽略。 |
| `setKeyCallback/pollEvents/shouldClose` | 输入和窗口关闭事件。 |
| `timeSeconds/sleepMilliseconds` | 单调时间和毫秒睡眠。 |
| `present` | 把 RGBA framebuffer 提交给后端窗口。 |

### 3.3 平台入口函数

| 函数 | 参数 | 返回值 | 用法和契约 |
| --- | --- | --- | --- |
| `GetPlatform()` | 无 | `const Platform *` | 返回当前平台函数表。未调用 `SetPlatform()` 时返回内置后端。 |
| `SetPlatform(platform)` | 平台表指针或 `nullptr` | 无 | 自定义后端必须在任何 HE3D 分配、窗口或文件操作前调用。传 `nullptr` 会清空当前表，下次重新取内置后端。 |
| `GetBuiltinPlatform()` | 无 | `const Platform *` | 当前编译后端提供的内置平台表。 |
| `Alloc(size)` | 字节数 | 指针或 `nullptr` | 通过当前平台分配内存。 |
| `Free(ptr)` | 指针 | 无 | 释放 `Alloc()` 得到的内存；`ptr == nullptr` 必须安全。 |
| `LoadFile(path, outFile)` | 路径、输出结构 | `bool` | 成功后 `outFile->data` 到 `CloseFile()` 前有效。失败返回 `false`。 |
| `CloseFile(file)` | 文件结构 | 无 | 释放文件数据，并把结构置为空状态。 |
| `CreateWindow(desc)` | 窗口描述 | `Window *` 或 `nullptr` | 创建窗口。失败后不能创建依赖该窗口的 renderer。 |
| `SetWindowTitle(window, title)` | 窗口、标题 | 无 | 改窗口标题。 |
| `DestroyWindow(window)` | 窗口 | 无 | 销毁窗口。 |
| `SetKeyCallback(window, callback, user)` | 窗口、回调、用户指针 | 无 | 注册键盘回调；`callback == nullptr` 表示取消。 |
| `PollEvents(window)` | 窗口 | 无 | 每帧调用。键盘回调只在这个函数执行期间交付。 |
| `WindowShouldClose(window)` | 窗口 | `bool` | 返回窗口是否请求关闭。 |
| `TimeSeconds()` | 无 | `double` | 单调秒数，必须有亚秒精度。 |
| `SleepMilliseconds(milliseconds)` | 毫秒数 | 无 | 睡眠指定毫秒数。 |
| `Present(window, width, height, pixels)` | 窗口、尺寸、RGBA 像素 | 无 | 后端提交 framebuffer。应用通常调用 `Renderer::Present()`，不直接调用它。 |

键盘约定：

- `pressed == true` 表示按下，`false` 表示松开。
- `Esc` 使用 ASCII `27`。
- 示例把 `'A'..'Z'` 规整到 `'a'..'z'` 后再写入按键表。
- 有真实 key-up 的后端必须交付真实 down/up。没有 key-up 的后端只能在 `PollEvents()` 中合成 release。
- 后端不应该用额外状态表过滤重复 key-down；应用层的 `keys[key] = pressed` 已经幂等。

时间约定：

- `TimeSeconds()` 单位是秒，不是毫秒或整数 tick。
- 返回值必须单调递增或持平，不应受系统墙上时间调整影响。
- 主循环用两次 `TimeSeconds()` 的差计算 `deltaTime`。
- `PaceFrame(frameStart)` 的 `frameStart` 必须来自同一个 `TimeSeconds()`。

### 3.4 帧率和抗锯齿全局函数

| 函数 | 参数 | 返回值 | 用法 |
| --- | --- | --- | --- |
| `SetFrameRateLimit(fps)` | FPS；`0` 表示不限制 | 无 | 设置全局限帧。 |
| `GetFrameRateLimit()` | 无 | 当前 FPS 限制 | `0` 表示不限帧。 |
| `PaceFrame(frameStart)` | 本帧开始秒数 | 无 | 如果设置了限帧，等待本帧剩余时间。主循环末尾调用。 |
| `SetFxaaEnabled(enabled)` | 开关 | 无 | 设置 FXAA。下一次 `Renderer::Present()` 生效。 |
| `IsFxaaEnabled()` | 无 | 当前开关 | 用于 UI 或按键 toggle。 |
| `SetTaaEnabled(enabled)` | 开关 | 无 | 设置 TAA。关闭时 renderer 会丢弃 TAA history。 |
| `IsTaaEnabled()` | 无 | 当前开关 | 用于 UI 或按键 toggle。 |
| `SetMsaaEnabled(enabled)` | 开关 | 无 | 设置 MSAA。之后绘制的三角形读取该状态。 |
| `IsMsaaEnabled()` | 无 | 当前开关 | 用于 UI 或按键 toggle。 |
| `SetSsaaScale(scale)` | 倍率 | 无 | 设置全局 SSAA 倍率，钳制到 `1..4`，`1` 表示关闭。 |
| `GetSsaaScale()` | 无 | 当前全局倍率 | 注意已有 renderer 不一定已经按该倍率重建缓冲。 |

SSAA 的生效点是 `Renderer` 构造或 `Renderer::Resize()`。运行时改 SSAA 后要这样写：

```cpp
HE3D::SetSsaaScale(2);
renderer.Resize(width, height);
```

## 4. 数学 API

数学 API 在 `include/he3d_math.h`，不依赖 libm 或平台 API，适合 XAPI freestanding 构建。

### 4.1 宏和常量

| 名称 | 用法 |
| --- | --- |
| `HE3D_MIN(a, b)` | 取较小值。 |
| `HE3D_MAX(a, b)` | 取较大值。 |
| `HE3D_CLAMP(x, lo, hi)` | 把 `x` 限制在 `[lo, hi]`。 |
| `HE3D_ABS(x)` | 绝对值宏。 |
| `HE3D_DEG2RAD(d)` | 角度转弧度。 |
| `HE3D_PI` | 圆周率。 |
| `HE3D_TAU` | `2 * PI`。 |
| `HE3D_PI_DIV_2` | `PI / 2`。 |

### 4.2 标量函数

| 函数 | 参数 | 返回值 | 说明 |
| --- | --- | --- | --- |
| `fabsf(x)` | `float` | `float` | 浮点绝对值。 |
| `abs(x)` | `int32_t` | `int32_t` | 整数绝对值。 |
| `floorf(x)` | `float` | `float` | 向下取整。 |
| `ceilf(x)` | `float` | `float` | 向上取整。 |
| `roundf(x)` | `float` | `float` | 四舍五入到整数值。 |
| `fracf(x)` | `float` | `float` | 返回用于环绕 UV 的小数部分，负数会映射到 `[0,1)`。 |
| `sqrtf_precise(x)` | `float` | `float` | 软件平方根，`x <= 0` 返回 `0`。 |
| `rsqrtf(x)` | `float` | `float` | 快速平方根倒数，`x <= 0` 返回 `0`。 |
| `sqrtf(x)` | `float` | `float` | 通过 `rsqrtf` 计算平方根，`x <= 0` 返回 `0`。 |
| `sinf(x)`、`cosf(x)`、`tanf(x)` | 弧度 | `float` | 软件三角函数。 |
| `sincosf(x, s, c)` | 弧度、输出指针 | 无 | 同时写出 sin 和 cos。 |
| `atan2f(y, x)` | 坐标分量 | 弧度 | 带象限修正的 `atan2` 近似。 |

### 4.3 向量、颜色、矩阵

| 类型 | 字段 | 构造 | 操作 |
| --- | --- | --- | --- |
| `float2` | `x, y` | `float2(x=0, y=0)` | `+`、`-`、标量 `* /`、分量 `*`。 |
| `float3` | `x, y, z` | `float3(x=0, y=0, z=0)` | `+`、`-`、一元 `-`、标量 `* /`、分量 `*`。 |
| `color3` | `r, g, b` | `color3(r=0, g=0, b=0)` | `+`、`-`、标量 `* /`、分量 `*`。 |
| `float4x4` | `m[16]` | 默认单位矩阵 | 当前只作为基础矩阵存储类型。 |

`float3` 额外函数：

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `lengthSq()` | `float` | 长度平方。 |
| `length()` | `float` | 长度。 |
| `normalize()` | `float3` | 归一化；接近零向量返回 `{0,0,0}`。 |
| `normalizeFast()` | `float3` | 使用 `rsqrtf` 的快速归一化。 |
| `float3::dot(a, b)` | `float` | 点乘。 |
| `float3::cross(a, b)` | `float3` | 叉乘。 |
| `float3::lerp(a, b, t)` | `float3` | 线性插值。 |
| `rotateX/Y/Z(angle)` | `float3` | 绕轴旋转，角度单位为弧度。 |
| `rotateX/Y/Z(s, c)` | `float3` | 使用调用者提供的 sin/cos 绕轴旋转。 |

### 4.4 射线和 AABB

| 类型 | 字段 | 默认值 |
| --- | --- | --- |
| `RayHit` | `hit, distance, position, normal, u, v` | `hit=false`，距离和向量为 0。 |
| `AABB` | `min, max` | 都为 `{0,0,0}`。 |
| `Ray` | `origin, direction` | 起点 `{0,0,0}`，方向 `{0,0,1}`。 |

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `AABB::Intersects(other)` | `bool` | 判断两个轴对齐包围盒是否重叠。 |
| `Ray::FromTo(from, to)` | `Ray` | 从 `from` 指向 `to`，方向会快速归一化。 |
| `Ray::Normalized()` | `Ray` | 返回方向归一化后的副本。 |
| `Ray::At(distance)` | `float3` | 返回 `origin + direction * distance`。 |
| `Ray::IntersectSphere(center, radius, outDistance)` | `bool` | 测试射线和球体，命中时可写距离。 |
| `Ray::IntersectPlane(point, normal, outDistance)` | `bool` | 测试射线和平面。射线平行或交点在反方向返回 `false`。 |
| `Ray::IntersectTriangle(v0, v1, v2, outDistance, outU, outV)` | `bool` | Moller-Trumbore 三角形相交测试，不剔除背面。 |
| `Ray::IntersectAABB(box, outNear, outFar)` | `bool` | 测试射线和 AABB，命中时可写进入/离开距离。 |
| `Ray::CastSphere()`、`CastPlane()`、`CastTriangle()` | `RayHit` | 返回完整命中信息；未命中返回默认 `RayHit`。 |

如果调用者要把 `distance` 当世界距离使用，`Ray::direction` 应该是单位向量。

### 4.5 四元数

| 函数或操作 | 返回值 | 说明 |
| --- | --- | --- |
| `quat(w=1, x=0, y=0, z=0)` | `quat` | 默认单位旋转。 |
| `quat::FromEuler(euler)` | `quat` | 从弧度欧拉角构造。`x=pitch`，`y=yaw`，`z=roll`。 |
| `quat::FromEulerFast(euler)` | `quat` | 快速路径，语义同上。 |
| `normalize()` | `quat` | 标准归一化；长度接近 0 返回单位旋转。 |
| `normalizeFast()` | `quat` | 使用 `rsqrtf` 的快速归一化。 |
| `operator*(q)` | `quat` | 组合旋转。 |
| `rotate(v)` | `float3` | 用四元数旋转向量。 |
| `inverse()` | `quat` | 返回共轭，适用于单位四元数。 |

## 5. 渲染资源和场景对象

这些类型在 `include/he3d.hpp`。

### 5.1 `DirectionalLight`

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `direction` | `{0, -1, 1}` | 光线方向，不是光源位置。 |
| `color` | `{1, 1, 1}` | 光颜色和强度。 |
| `ambient` | `0.15f` | 环境光强度。 |

通过 `renderer.SetMainLight(light)` 设置主光源。

### 5.2 `Mesh`

字段：

| 字段 | 说明 |
| --- | --- |
| `vertices` | 顶点数组。每 3 个连续顶点组成一个三角形。 |
| `uvs` | UV 数组，容量和顶点数组一致。 |
| `triNormals` | 每个三角形一条法线。 |
| `vertCount` | 当前有效顶点数。 |
| `capacity` | 已分配顶点容量。 |

函数：

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `Mesh()` | 对象 | 创建空 mesh。 |
| `~Mesh()` | 无 | 释放 `vertices`、`uvs`、`triNormals`。 |
| `Init(vertexCount)` | `bool` | 分配顶点、UV 和三角形法线数组。`vertexCount <= 0` 失败。 |
| `Init(srcVertices, srcUvs, vertexCount)` | `bool` | 分配并复制顶点/UV。`srcUvs == nullptr` 时 UV 填 0。 |
| `RecalculateTriangleNormals()` | 无 | 每 3 个顶点重算一条三角形法线。 |
| `Mesh::Create(vertexCount)` | `Mesh *` | 创建并初始化 mesh。失败返回 `nullptr`。 |
| `Mesh::Create(srcVertices, srcUvs, vertexCount)` | `Mesh *` | 创建并复制数据。失败返回 `nullptr`。 |
| `Mesh::LoadOBJ(filename)` | `Mesh *` | 加载 OBJ。失败返回 `nullptr`。 |

OBJ 支持：

- `v` 顶点位置。
- `vt` UV。
- `f` 三角形或多边形，加载时扇形三角化。
- face 索引格式支持 `v/vt`、`v//vn`、`v/vt/vn`。
- `vt` 的 V 分量会转换为 `1 - rawV`，匹配 HE3D 纹理采样坐标。
- 不支持负索引；单个 face 最多 32 个顶点；法线会由 HE3D 重算。

### 5.3 `Texture`

字段：

| 字段 | 说明 |
| --- | --- |
| `width`、`height` | 像素尺寸。 |
| `pixels` | `ColorA` RGBA8 像素数组。 |
| `valid` | 成功加载后为 `true`。 |

函数：

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `Texture()` | 对象 | 创建空纹理。 |
| `~Texture()` | 无 | 释放 `pixels`。 |
| `Sample(u, v)` | `color3` | 用最近邻采样并环绕 UV；无效纹理返回洋红色。 |
| `Texture::LoadBMP(filename)` | `Texture *` | 加载 BMP，成功后调用者 `delete`。 |
| `Texture::LoadPNG(filename)` | `Texture *` | 加载 PNG 并转 RGBA8。 |
| `Texture::LoadJPG(filename)` | `Texture *` | 加载 JPG/JPEG 并转 RGBA8。 |
| `Texture::LoadImage(filename)` | `Texture *` | 按文件头识别 BMP、PNG、JPG/JPEG。 |

图片限制：

- BMP 支持未压缩 24-bit BGR 和 32-bit BGRA，支持 top-down/bottom-up。
- PNG/JPG 通过内置解码器读取，统一转 RGBA8。
- 单边最大 8192 像素。
- 解码失败、尺寸非法或分配失败返回 `nullptr`。

### 5.4 `GameObject` 和 `Camera`

| 类型 | 字段 | 说明 |
| --- | --- | --- |
| `GameObject` | `mesh, position, orientation` | mesh 实例。默认 `mesh=nullptr`、位置为 0、旋转为单位四元数。`GameObject` 不拥有 mesh。 |
| `Camera` | `position, orientation, fov` | 相机。默认位置 `{0,0,5}`，看向本地 +Z，`fov` 默认 90 度，是垂直视场角。 |

`GameObject::Forward()` 返回对象本地 +Z 经过 `orientation` 旋转后的世界方向。

### 5.5 `Renderer`

构造：

```cpp
HE3D::Renderer renderer(window, width, height);
```

`window` 必须是有效 `Window *`。`width` 和 `height` 必须大于 0。

函数：

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `Renderer(window, w, h)` | 对象 | 创建 renderer 并分配缓冲。 |
| `~Renderer()` | 无 | 释放 renderer 内部缓冲。 |
| `Clear(color)` | 无 | 开始新帧，清空 color/depth buffer。每帧绘制前调用一次。 |
| `DrawGameObject(obj, cam, color)` | 无 | 用纯色绘制对象。`obj.mesh` 必须有效。 |
| `DrawGameObject(obj, cam, texture)` | 无 | 用纹理绘制对象。`texture.valid` 应为 `true`。 |
| `Present()` | 无 | 执行 FXAA/TAA/SSAA 解析并提交窗口。 |
| `Resize(w, h)` | 无 | 重建 renderer 缓冲。窗口大小或 SSAA 倍率变化后调用。 |
| `SetMainLight(light)` | 无 | 设置主方向光。 |

典型帧顺序固定为：

```cpp
HE3D::PollEvents(window);
renderer.Clear({0.1f, 0.1f, 0.12f});
renderer.DrawGameObject(object, camera, textureOrColor);
renderer.Present();
```

## 6. 抗锯齿

HE3D 抗锯齿只通过全局函数控制，不直接调用内部 pass。

初始化：

```cpp
HE3D::SetFxaaEnabled(true);
HE3D::SetTaaEnabled(false);
HE3D::SetMsaaEnabled(false);
HE3D::SetSsaaScale(1);
HE3D::Renderer renderer(window, width, height);
```

运行时切换：

```cpp
HE3D::SetFxaaEnabled(!HE3D::IsFxaaEnabled());
HE3D::SetTaaEnabled(!HE3D::IsTaaEnabled());
HE3D::SetMsaaEnabled(!HE3D::IsMsaaEnabled());

HE3D::SetSsaaScale(2);
renderer.Resize(width, height);
```

函数说明：

| 功能 | 函数 | 生效时机 |
| --- | --- | --- |
| FXAA | `SetFxaaEnabled()` / `IsFxaaEnabled()` | `Renderer::Present()` 读取当前值。 |
| TAA | `SetTaaEnabled()` / `IsTaaEnabled()` | `Renderer::Present()` 读取当前值；关闭会清空 history 状态。 |
| MSAA | `SetMsaaEnabled()` / `IsMsaaEnabled()` | 三角形光栅化时读取当前值，影响之后绘制的三角形。 |
| SSAA | `SetSsaaScale()` / `GetSsaaScale()` | `Renderer` 构造或 `Renderer::Resize()` 时复制倍率到 renderer 内部缓冲尺寸。 |

执行顺序：

1. MSAA 在 `DrawGameObject()` 的三角形光栅化阶段做 4-sample 覆盖估算。
2. FXAA 在 `Present()` 中先处理当前颜色缓冲。
3. TAA 在 `Present()` 中使用当前颜色、上一帧颜色 history、上一帧 depth history 和 jitter。
4. SSAA 在 `Present()` 最后把高分辨率内部缓冲降采样到窗口尺寸。

成本说明：

- FXAA 成本低，但可能轻微模糊高对比边缘。
- TAA 改善稳定边缘，但快速运动可能有拖影。
- MSAA 当前实现仍使用单像素 depth，不是完整 per-sample depth/color MSAA。
- SSAA 的 2x 线性倍率约 4 倍像素，4x 线性倍率约 16 倍像素。

## 7. 碰撞和物理

### 7.1 `CollisionBox`

`CollisionBox` 是可绑定到 `GameObject` 的 OBB。

| 字段 | 说明 |
| --- | --- |
| `object` | 绑定对象；不拥有它。 |
| `centerOffset` | 相对对象位置的局部中心偏移。 |
| `halfExtents` | 三轴半尺寸，三个分量都大于 0 时有效。 |

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `CollisionBox()` | 对象 | 创建空 box。 |
| `CollisionBox(gameObject)` | 对象 | 创建并绑定对象。 |
| `BindGameObject(gameObject)` | 无 | 修改绑定对象。 |
| `FitMesh()` | `bool` | 根据绑定对象 mesh 拟合局部 AABB。失败会清空 box。 |
| `FitMesh(mesh)` | `bool` | 根据指定 mesh 拟合。 |
| `Center()` | `float3` | 世界空间中心。 |
| `Orientation()` | `quat` | 世界空间方向。 |
| `IsValid()` | `bool` | `halfExtents` 是否有效。 |
| `Contains(point)` | `bool` | 判断世界点是否在 OBB 内。 |
| `Intersects(other)` | `bool` | 判断两个 OBB 是否相交。 |
| `Contact(other, outContact)` | `bool` | SAT 接触测试，命中时可写接触信息。 |
| `Raycast(ray)` | `RayHit` | 射线查询。 |
| `WorldAABB()` | `AABB` | 返回包围该 OBB 的世界 AABB。 |

### 7.2 `PhysicsContact` 和 `PhysicsMaterial`

| 类型 | 字段 | 说明 |
| --- | --- | --- |
| `PhysicsContact` | `hit, penetration, normal, point` | 碰撞接触结果。默认 `hit=false`。 |
| `PhysicsMaterial` | `restitution, friction, linearDamping, angularDamping, drag` | 物理响应参数。 |

### 7.3 `HeightFieldCollider`

高度场用回调采样：

```cpp
static float TerrainHeight(float x, float z, void *user) {
    return ...;
}

HE3D::HeightFieldCollider collider(TerrainHeight, user, gridCount, cellSize);
```

| 字段 | 说明 |
| --- | --- |
| `sampleHeight` | 高度采样回调。 |
| `sampleUser` | 传给回调的用户指针。 |
| `gridCount` | 每个 tile 单边采样点数量，至少 2。 |
| `cellSize` | 网格间距，必须大于 0。 |

`HeightFieldCollider::Tile` 是缓存结构，用来保存一个 tile 的 cell AABB。

| `Tile` 字段或函数 | 说明 |
| --- | --- |
| `active` | 该 tile 缓存是否可用。 |
| `originX`、`originZ` | tile 世界空间起点。 |
| `bounds` | 整个 tile 的 AABB。 |
| `cellBounds` | 每个 cell 的 AABB 缓存数组。 |
| `cellCount`、`cellCapacity` | 当前 cell 数和容量。 |
| `Tile()` / `~Tile()` | 构造空 tile；析构释放 `cellBounds`。 |
| `EnsureCapacity(count)` | 确保 `cellBounds` 至少能容纳 `count` 个 cell。 |

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `HeightFieldCollider()` | 对象 | 创建无效高度场。 |
| `HeightFieldCollider(callback, user, gridCount, cellSize)` | 对象 | 创建并配置高度场。 |
| `Configure(callback, user, gridCount, cellSize)` | 无 | 重新配置高度场。 |
| `IsValid()` | `bool` | 回调非空、`gridCount >= 2`、`cellSize > 0` 时为真。 |
| `BuildTile(tile, originX, originZ)` | `bool` | 为 tile 建立 cell AABB 缓存。 |
| `ContactBox(box, sweep, outContact)` | `bool` | 直接采样高度场并测试 box。 |
| `ContactBox(box, tiles, tileCount, sweep, outContact)` | `bool` | 使用 tile 缓存过滤候选 cell，再测试 box。 |
| `NormalAt(x, z)` | `float3` | 通过相邻高度估算法线。 |

`sweep > 0` 会扩展查询 AABB，适合高速物体减少穿模。

### 7.4 `CollisionBoxSet`

`CollisionBoxSet` 用多个 box 近似一个 mesh。

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `CollisionBoxSet()` | 对象 | 创建空 box 集合。 |
| `CollisionBoxSet(gameObject)` | 对象 | 创建并绑定对象。 |
| `~CollisionBoxSet()` | 无 | 释放内部 box 数组。 |
| `BindGameObject(gameObject)` | 无 | 绑定对象。 |
| `FitMeshGrid(xParts, yParts, zParts)` | `bool` | 按网格划分绑定对象 mesh。三个分段数必须大于 0。 |
| `FitMeshGrid(mesh, xParts, yParts, zParts)` | `bool` | 按网格划分指定 mesh。 |
| `IsValid()` | `bool` | 是否有有效 box。 |
| `Contains(point)` | `bool` | 任一 box 包含该点即返回 `true`。 |
| `Intersects(other)` | `bool` | 任一 box 和 `other` 相交即返回 `true`。 |
| `Raycast(ray)` | `RayHit` | 返回最近命中。 |

独立查询函数：

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `RaycastCollisionBox(ray, box, outHit)` | `bool` | 查询单个 box。 |
| `RaycastCollisionBoxes(ray, boxes, boxCount, outHit, outIndex)` | `bool` | 查询 box 数组，返回最近命中和下标。 |
| `RaycastCollisionBoxSet(ray, boxSet, outHit, outIndex)` | `bool` | 查询 boxSet。 |
| `RaycastMeshTriangles(ray, object, outHit, outTriangleIndex)` | `bool` | 遍历 mesh 三角形，按对象 transform 查询最近命中。 |

### 7.5 `PhysicsBody`

`PhysicsBody` 是绑定到 `GameObject` 的轻量运动积分器。

| 字段 | 说明 |
| --- | --- |
| `object` | 被移动的对象；不拥有它。 |
| `velocity`、`angularVelocity` | 线速度和角速度。 |
| `force`、`torque` | 下一次积分使用的累计力和力矩。 |
| `gravity` | 重力加速度。 |
| `mass/inverseMass`、`inertia/inverseInertia` | 质量和转动惯量及其倒数。 |
| `linearDamping`、`angularDamping`、`drag` | 阻尼和阻力。 |
| `restitution`、`friction` | 碰撞响应参数。 |
| `material` | 材质参数副本。 |

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `PhysicsBody()` | 对象 | 创建未绑定物理体。 |
| `PhysicsBody(gameObject)` | 对象 | 创建并绑定对象。 |
| `BindGameObject(gameObject)` | 无 | 绑定对象。 |
| `SetEnabled(enabled)` / `IsEnabled()` | 无 / `bool` | 开关积分。 |
| `SetMass(value)` | 无 | `value > 0` 设置质量和倒数，否则倒数为 0。 |
| `SetInertia(value)` | 无 | `value > 0` 设置惯量和倒数，否则倒数为 0。 |
| `SetMaterial(value)` | 无 | 复制材质，并同步响应字段。 |
| `SetVelocity(value)` | 无 | 设置线速度。 |
| `SetAngularVelocity(value)` | 无 | 设置角速度。 |
| `AddForce(value)` | 无 | 累加到下一次 `Step()`。 |
| `AddTorque(value)` | 无 | 累加到下一次 `Step()`。 |
| `AddImpulse(impulse)` | 无 | 立即按 `inverseMass` 改变速度。 |
| `AddAngularImpulse(impulse)` | 无 | 立即按 `inverseInertia` 改变角速度。 |
| `ClearForces()` / `ClearTorques()` | 无 | 清空累计力或力矩。 |
| `Step(deltaTime)` | 无 | 积分速度、位置和姿态；完成后清空力和力矩。 |
| `StepWithCollisions(...)` | `bool` | 分步积分并解析 box 障碍碰撞。 |
| `ResolveHeightField(...)` | `bool` | 解析高度场接触。 |

`Step(deltaTime)` 在 `deltaTime <= 0`、未启用或未绑定对象时直接返回。

## 8. 内置后端

| 后端 | 行为 |
| --- | --- |
| XAPI | 创建 XJ380 GUI 窗口，通过 `xapi_WriteBufferA` 提交 `ColorA` 缓冲区。消息线程记录 `MSG_KEYDOWN` / `MSG_KEYUP`，应用调用 `PollEvents()` 时交付回调。 |
| SDL3 | 创建 SDL 窗口、renderer 和 `SDL_PIXELFORMAT_RGBA32` streaming texture。转发 SDL key down/up；窗口失焦时释放记录为按下的键。 |
| CONSOLE | 使用 ANSI 24-bit 颜色和上半格字符显示 framebuffer。终端没有真实 key-up，因此在重复输入停止后合成 release。 |

后端作者必须满足：

- `alloc/free` 可用于 HE3D 全部对象；`free(nullptr)` 安全。
- `loadFile` 成功时返回完整文件内容，失败时不留下所有权。
- `timeSeconds` 返回有亚秒精度的单调秒数。
- `sleepMilliseconds` 按毫秒参数睡眠，不能把小于 1000 ms 的请求当成 0 秒或 1 秒。
- `present` 接收内存顺序 RGBA 像素。

## 9. 内置示例

| 示例 | 目录 | 作用 | 控制 |
| --- | --- | --- | --- |
| `TriangleTest` | `examples/TriangleTest/` | 最小程序：窗口、回调、mesh、renderer、主循环、释放资源。 | `Esc` 退出。 |
| `AirplaneTest` | `examples/AirplaneTest/` | 加载大型 OBJ，测试 renderer 和抗锯齿开关。 | `T` 切 TAA，`M` 切 MSAA，`1..4` 设置 SSAA，`Esc` 退出。 |
| `PhysicsTest` | `examples/PhysicsTest/` | 物理示例：重力、碰撞箱、地面接触、力、扭矩和冲量。 | `A/D` 施加水平力，`Space` 跳跃，`R` 重置，`F` 切 FXAA，`Esc` 退出。 |
| `FlightSimulator` | `examples/FlightSimulator/` | 飞行示例：输入、相机、OBJ/BMP、地形 tile、高度场接触、物理控制。 | `W/S` 俯仰，`Q/E` 偏航，`A/D` 滚转，`F` 切 FXAA，`Esc` 退出。 |

FlightSimulator 关键实现约定：

- 输入只来自 `SetKeyCallback()` 写入的 `keys[256]` 状态表。
- 字母键在回调里规整为小写；主循环仍兼容大小写索引。
- 飞行手感使用 `InputCurve()` 平滑键盘输入，再用增量四元数更新姿态。
- 飞机前进速度来自 `plane.Forward() * 15.0f`。
- 地形渲染 tile 和 `HeightFieldCollider::Tile` 同步生成。
- 每帧物理积分后调用 `ResolveHeightField()`，并用速度相关 sweep 减少穿模。
