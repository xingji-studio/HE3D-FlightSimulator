# HE3D 技术手册

本文档列出 HE3D 对用户开放的接口：构建选项、公开头文件、平台服务、数学类型、渲染对象、资源格式和飞行模拟器示例。

## 构建

HE3D 通过 CMake 一次构建一个后端。

```sh
cmake -S . -B build -DHE3D_BACKEND=XAPI
cmake --build build
```

```sh
cmake -S . -B build -DHE3D_BACKEND=SDL3
cmake --build build
```

`HE3D_BACKEND` 可选值：

- `XAPI`：构建 XJ380 程序 `he3d_flight_simulator.elf`。
- `SDL3`：构建桌面 SDL3 程序 `he3d_flight_simulator_sdl3`。
- `CONSOLE`：构建 `he3d_flight_simulator_console`，使用控制台后端在终端里显示飞行模拟器。

生成的可执行文件和复制的资源文件都放在 `build/`。切换后端时重新配置同一个 `build/` 目录。

## 公开头文件

- `include/he3d.hpp`：引擎对象、渲染器、网格、纹理、相机、光照。
- `include/he3d_platform.hpp`：引擎使用的平台接口。
- `include/he3d_math.h`：标量数学、向量、四元数、矩阵类型。

公开 C++ 类型都位于 `HE3D` 命名空间。HE3D 提供全局 `new` 和 `delete`，内存分配会转发到当前平台。

## 数学 API

`he3d_math.h` 可在 freestanding 环境使用，不依赖 libm。

标量函数：

- `fabsf(float x) -> float`
- `abs(int x) -> int`
- `floorf(float x) -> float`
- `ceilf(float x) -> float`
- `roundf(float x) -> float`
- `fracf(float x) -> float`
- `sqrtf(float x) -> float`：通过快速平方根倒数计算。
- `rsqrtf(float x) -> float`：快速平方根倒数。
- `sinf(float x) -> float`
- `cosf(float x) -> float`
- `tanf(float x) -> float`
- `sincosf(float x, float *s, float *c)`
- `atan2f(float y, float x) -> float`

宏和常量：

- `HE3D_MIN(a, b)`
- `HE3D_MAX(a, b)`
- `HE3D_CLAMP(x, lo, hi)`
- `HE3D_ABS(x)`
- `HE3D_DEG2RAD(d)`
- `HE3D_PI`
- `HE3D_TAU`
- `HE3D_PI_DIV_2`

`float2` 保存两个标量：

```cpp
struct float2 {
    float x, y;
};
```

支持的操作：

- 构造：`float2(float x = 0, float y = 0)`
- `+`、`-`
- 标量 `*` 和 `/`
- 分量相乘 `*`

`float3` 保存三维向量：

```cpp
struct float3 {
    float x, y, z;
};
```

支持的操作：

- 构造：`float3(float x = 0, float y = 0, float z = 0)`
- `+`、`-`、一元 `-`
- 标量 `*` 和 `/`
- 分量相乘 `*`
- `lengthSq()`
- `length()`
- `normalize()`
- `normalizeFast()`：使用 `rsqrtf`，用于允许少量归一化误差的热路径。
- `float3::dot(a, b)`
- `float3::cross(a, b)`
- `float3::lerp(a, b, t)`
- `rotateX(angle)`、`rotateY(angle)`、`rotateZ(angle)`
- `rotateX(s, c)`、`rotateY(s, c)`、`rotateZ(s, c)`

`color3` 保存线性 RGB 颜色：

```cpp
struct color3 {
    float r, g, b;
};
```

支持的操作：

- 构造：`color3(float r = 0, float g = 0, float b = 0)`
- `+`、`-`
- 标量 `*` 和 `/`
- 分量相乘 `*`

`quat` 表示旋转：

```cpp
struct quat {
    float w, x, y, z;
};
```

支持的操作：

- 构造：`quat(float w = 1, float x = 0, float y = 0, float z = 0)`
- `quat::FromEuler(float3 euler)`
- `quat::FromEulerFast(float3 euler)`
- `normalize()`
- `normalizeFast()`：使用 `rsqrtf`，用于每帧姿态清理，不作为精确数学接口。
- 四元数乘法 `operator*`
- `rotate(const float3& v)`
- `inverse()`

欧拉角单位是弧度。`FromEuler` 使用 X 作为 pitch，Y 作为 yaw，Z 作为 roll。

## 射线 API

`Ray` 用于 3D 查询。方向向量建议传入单位向量，或者使用 `Ray::FromTo()` / `Normalized()` 创建。

```cpp
HE3D::Ray ray({0, 1, -5}, {0, 0, 1});
float distance = 0.0f;
if (ray.IntersectSphere({0, 1, 0}, 1.0f, &distance)) {
    HE3D::float3 hitPoint = ray.At(distance);
}
```

类型：

- `Ray`：`origin`、`direction`
- `RayHit`：`hit`、`distance`、`position`、`normal`、`u`、`v`
- `AABB`：`min`、`max`

函数：

- `Ray::FromTo(from, to) -> Ray`
- `Normalized() -> Ray`
- `At(distance) -> float3`
- `IntersectSphere(center, radius, outDistance) -> bool`
- `IntersectPlane(point, normal, outDistance) -> bool`
- `IntersectTriangle(v0, v1, v2, outDistance, outU, outV) -> bool`
- `IntersectAABB(box, outNear, outFar) -> bool`
- `CastSphere(center, radius) -> RayHit`
- `CastPlane(point, normal) -> RayHit`
- `CastTriangle(v0, v1, v2) -> RayHit`

`float4x4` 是初始化为单位矩阵的 4x4 矩阵：

```cpp
struct float4x4 {
    float m[16];
};
```

## 平台 API

平台层向引擎提供内存、文件、窗口、输入、时间和 framebuffer 提交。

字节类型：

```cpp
typedef /* 8-bit unsigned integer */ uint8_t;
```

像素布局：

```cpp
struct ColorA {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};
```

文件数据：

```cpp
struct FileData {
    void *handle;
    const uint8_t *data;
    unsigned long long length;
};
```

窗口创建：

```cpp
struct Window;

struct WindowDesc {
    int width;
    int height;
    const char *title;
    unsigned int flags;
};
```

键盘回调：

```cpp
typedef void (*KeyCallback)(int key, bool pressed, void *user);
```

平台函数表：

```cpp
struct Platform {
    void *(*alloc)(unsigned long size);
    void  (*free)(void *ptr);

    bool  (*loadFile)(const char *path, FileData *outFile);
    void  (*closeFile)(FileData *file);

    Window *(*createWindow)(const WindowDesc *desc);
    void    (*setWindowTitle)(Window *window, const char *title);
    void    (*destroyWindow)(Window *window);
    void    (*setKeyCallback)(Window *window, KeyCallback callback, void *user);
    void    (*pollEvents)(Window *window);
    bool    (*shouldClose)(Window *window);
    double  (*timeSeconds)();
    void    (*sleepMilliseconds)(unsigned long long milliseconds);
    void    (*present)(Window *window, int width, int height, const ColorA *pixels);
};
```

平台入口：

- `GetPlatform() -> const Platform *`
- `SetPlatform(const Platform *platform)`
- `GetBuiltinPlatform() -> const Platform *`
- `Alloc(unsigned long size) -> void *`
- `Free(void *ptr)`
- `LoadFile(const char *path, FileData *outFile) -> bool`
- `CloseFile(FileData *file)`
- `CreateWindow(const WindowDesc *desc) -> Window *`
- `SetWindowTitle(Window *window, const char *title)`
- `DestroyWindow(Window *window)`
- `SetKeyCallback(Window *window, KeyCallback callback, void *user)`
- `PollEvents(Window *window)`
- `WindowShouldClose(Window *window) -> bool`
- `TimeSeconds() -> double`
- `SleepMilliseconds(unsigned long long milliseconds)`
- `SetFrameRateLimit(unsigned int fps)`
- `GetFrameRateLimit() -> unsigned int`
- `SetFxaaEnabled(bool enabled)`
- `IsFxaaEnabled() -> bool`
- `PaceFrame(double frameStart)`
- `Present(Window *window, int width, int height, const ColorA *pixels)`

`LoadFile` 返回的数据在调用 `CloseFile` 前有效。`CreateWindow` 返回的窗口必须用 `DestroyWindow` 释放。

## 自定义后端

后端是一个提供 `Platform` 函数表的编译单元。作为目标内置后端使用时，它还要定义 `HE3D::GetBuiltinPlatform()`。

仓库里已经包含一个完整控制台后端：`src/platform/he3d_platform_console.cpp`。它使用 C++ 标准库处理内存、文件、时间和终端输出。它用 ANSI 24-bit 颜色和上半格字符显示 renderer buffer：前景色是采样后的上方像素，背景色是采样后的下方像素。后端会先把 framebuffer 缩放到适合终端的尺寸再输出。

构建和运行：

```sh
cmake -S . -B build -DHE3D_BACKEND=CONSOLE
cmake --build build
./build/he3d_flight_simulator_console
```

自定义后端有两种接入方式：

- 作为目标后端链接：在自定义后端源文件里定义 `GetBuiltinPlatform()`。同一个目标里不要再链接其他同样定义 `GetBuiltinPlatform()` 的后端文件。
- 运行时替换平台：链接已有后端，然后在创建窗口、加载资源或分配引擎对象之前调用 `SetPlatform(&myPlatform)`。

后端文件通常使用下面的结构。控制台后端就是这个结构的完整实现：

`he3d_platform.hpp` 里的 `Window` 是不透明类型。真正的 `struct Window` 只在后端自己的源文件里定义。应用代码只从 `CreateWindow` 拿到 `Window *`，然后把这个指针继续传给 HE3D 的函数。

```cpp
#include "he3d_platform.hpp"

namespace HE3D {

struct Window {
    /* 后端自己的窗口状态 */
};

static void *MyAlloc(unsigned long size) { /* ... */ }
static void  MyFree(void *ptr) { /* ... */ }

static bool MyLoadFile(const char *path, FileData *outFile) { /* ... */ }
static void MyCloseFile(FileData *file) { /* ... */ }

static Window *MyCreateWindow(const WindowDesc *desc) { /* ... */ }
static void MySetWindowTitle(Window *window, const char *title) { /* ... */ }
static void MyDestroyWindow(Window *window) { /* ... */ }
static void MySetKeyCallback(Window *window, KeyCallback callback, void *user) { /* ... */ }
static void MyPollEvents(Window *window) { /* ... */ }
static bool MyShouldClose(Window *window) { /* ... */ }
static double MyTimeSeconds() { /* ... */ }
static void MyPresent(Window *window, int width, int height, const ColorA *pixels) { /* ... */ }

static const Platform g_myPlatform = {
    MyAlloc,
    MyFree,
    MyLoadFile,
    MyCloseFile,
    MyCreateWindow,
    MySetWindowTitle,
    MyDestroyWindow,
    MySetKeyCallback,
    MyPollEvents,
    MyShouldClose,
    MyTimeSeconds,
    MyPresent
};

const Platform *GetBuiltinPlatform()
{
    return &g_myPlatform;
}

}
```

应用代码不直接构造 `Window`：

```cpp
HE3D::WindowDesc desc = {};
desc.width = 800;
desc.height = 600;
desc.title = "Example";

HE3D::Window *window = HE3D::CreateWindow(&desc);
HE3D::Renderer renderer(window, desc.width, desc.height);

while (!HE3D::WindowShouldClose(window)) {
    HE3D::PollEvents(window);
    renderer.Clear(HE3D::color3(0.1f, 0.1f, 0.12f));
    renderer.Present();
}

HE3D::DestroyWindow(window);
```

后端回调契约：

- `alloc` 返回可用于任意 HE3D 对象的内存。`free` 释放 `alloc` 返回的内存，并应接受 `nullptr`。
- `loadFile` 成功时填写 `outFile->handle`、`outFile->data`、`outFile->length`。失败时返回 `false`，且不留下需要释放的数据。
- `closeFile` 释放 `loadFile` 返回的数据。
- `createWindow` 分配并返回后端拥有的 `Window *`。调用方把它当作不透明句柄使用。
- `setWindowTitle` 可以忽略不支持的标题修改，但要能接受有效窗口和标题。
- `destroyWindow` 释放窗口拥有的全部资源。
- `setKeyCallback` 保存回调函数和用户指针，供输入事件使用。
- `pollEvents` 轮询宿主事件队列，并调用已保存的键盘回调。
- `shouldClose` 在用户关闭窗口或后端失败后返回 `true`。
- `timeSeconds` 返回单调递增的秒数。
- `present` 接收 `width * height` 个按行排列的 `ColorA` 像素。调用返回后，后端不拥有这块内存。

键盘回调使用整数键值。可打印按键应使用 ASCII 码，Escape 使用 `27`。有独立 key-up 事件的后端应同时转发按下和释放；只收到按下消息的后端可以在 `pollEvents` 里合成释放。

`ColorA` 像素是 RGBA 字节顺序。如果宿主 API 使用不同字节顺序或 stride，在 `present` 内转换或上传。

CMake 目标需要编译 `src/he3d.cpp`、自定义后端源文件和应用源文件。每个目标只能链接一个提供 `GetBuiltinPlatform()` 的后端实现。

控制台后端在 CMake 里这样接入：

```cmake
add_library(he3d_engine_console STATIC
    src/he3d.cpp
    src/platform/he3d_platform_console.cpp
)

target_include_directories(he3d_engine_console
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(he3d_flight_simulator_console
    ${HE3D_FLIGHT_SOURCES}
)

target_link_libraries(he3d_flight_simulator_console
    PRIVATE
        he3d_engine_console
)
```

控制台目标和 SDL3/XAPI 目标使用同一份飞行模拟器源码。应用仍然是填写 `WindowDesc`，调用 `CreateWindow`，用返回的 `Window *` 构造 `Renderer`，绘制并 `Present`，最后调用 `DestroyWindow`。

## 引擎 API

`DirectionalLight` 控制方向光：

```cpp
struct DirectionalLight {
    float3 direction;
    color3 color;
    float ambient;
};
```

默认值是方向 `{0, -1, 1}`，颜色 `{1, 1, 1}`，环境光 `0.15f`。

`Mesh` 拥有三角形顶点数据：

```cpp
class Mesh {
public:
    float3 *vertices;
    float2 *uvs;
    float3 *triNormals;
    int vertCount;
    int capacity;
};
```

Mesh 函数：

- `bool Init(int vertexCount)`
- `bool Init(const float3 *srcVertices, const float2 *srcUvs, int vertexCount)`
- `void RecalculateTriangleNormals()`
- `static Mesh *Create(int vertexCount)`
- `static Mesh *Create(const float3 *srcVertices, const float2 *srcUvs, int vertexCount)`
- `static Mesh *LoadOBJ(const char *filename)`

`vertCount` 是当前有效顶点数。HE3D 绘制三角形，因此顶点按每 3 个一组三角形解释。`capacity` 是已分配顶点数，可用于原地重填 mesh。

`Texture` 拥有 RGBA 字节像素：

```cpp
class Texture {
public:
    int width;
    int height;
    ColorA *pixels;
    bool valid;
};
```

Texture 函数：

- `color3 Sample(float u, float v) const`
- `static Texture *LoadBMP(const char *filename)`

`Sample` 会环绕 UV，并使用最近邻采样。无效纹理采样结果是洋红色 `{1, 0, 1}`。

`GameObject` 把 mesh 绑定到变换：

```cpp
class GameObject {
public:
    Mesh *mesh;
    float3 position;
    quat orientation;
};
```

GameObject 函数：

- `float3 Forward() const`

`Forward()` 返回局部 `{0, 0, 1}` 经过 `orientation` 旋转后的方向。

`Camera` 保存视图变换和视场角：

```cpp
class Camera {
public:
    float3 position;
    quat orientation;
    float fov;
    float targetFov;
    float zoomSpeed;
};
```

Camera 函数：

- `void Update(float deltaTime)`

`Update` 会让 `fov` 按 `zoomSpeed` 向 `targetFov` 靠近。

`Renderer` 将 mesh 绘制到平台窗口：

```cpp
class Renderer {
public:
    DirectionalLight mainLight;

    Renderer(Window *window, int w, int h);
    ~Renderer();

    void Clear(color3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, color3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, const Texture& tex);
    void Present();
    void Resize(int w, int h);
    void SetMainLight(const DirectionalLight& light);
};
```

每帧调用 `Clear`，绘制所有对象，然后调用一次 `Present`。`Resize` 会重新分配 color 和 depth buffer。

## 资源格式

`Mesh::LoadOBJ` 支持常见静态 OBJ 网格数据：

- `v x y z`
- `vt u v`
- 使用顶点索引和可选纹理坐标索引的 polygon face
- 超过三个顶点的 face 使用扇形三角化

OBJ 法线不是必需项。HE3D 会为输出的每个三角形重新计算一条法线。

`Texture::LoadBMP` 支持：

- 未压缩 BMP
- 24-bit BGR
- 32-bit BGRA
- top-down 和 bottom-up 行顺序
- 单边最大 8192 像素

## 内置后端

XAPI 后端创建 XJ380 GUI 窗口，并通过 `xapi_WriteBufferA` 提交 `ColorA` 缓冲区。键盘输入优先使用 `MSG_KEYDOWN` 和 `MSG_KEYUP`，旧的 `MSG_CHAR` / `MSG_SPCHAR` 路径只作为兼容兜底。

SDL3 后端创建 SDL 窗口、renderer 和 `SDL_PIXELFORMAT_RGBA32` streaming texture。SDL 的 key down/up 事件会直接传给 HE3D 键盘回调。

## 飞行模拟器示例

示例位于 `examples/FlightSimulator/`。

控制：

- `W` / `S`：俯仰
- `Q` / `E`：偏航
- `A` / `D`：滚转
- `Esc`：退出

资源：

- `examples/FlightSimulator/assets/biplane.obj`
- `examples/FlightSimulator/assets/biplane.bmp`

构建时这两个文件会复制到可执行文件同目录。如果 `biplane.obj` 无法加载，示例会使用内置备用飞机网格。

示例在飞机周围维护 3x3 地形 tile。地形 mesh 会原地更新；飞机进入新 tile 坐标后，每帧最多生成一个缺失 tile。

## 所有权规则

- `Mesh::Create` 和 `Mesh::LoadOBJ` 返回的对象用 `delete` 释放。
- `Texture::LoadBMP` 返回的对象用 `delete` 释放。
- `LoadFile` 返回的数据用 `CloseFile` 释放。
- `CreateWindow` 返回的窗口用 `DestroyWindow` 释放。
- `GameObject` 和 `Camera` 不拥有 mesh 或 texture。
