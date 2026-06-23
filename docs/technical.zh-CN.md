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
- `sqrtf(float x) -> float`
- `rsqrtf(float x) -> float`
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

`float3` 保存三维向量或 RGB 颜色：

```cpp
struct float3 {
    union {
        struct { float x, y, z; };
        struct { float r, g, b; };
    };
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
- `normalizeFast()`
- `float3::dot(a, b)`
- `float3::cross(a, b)`
- `float3::lerp(a, b, t)`
- `rotateX(angle)`、`rotateY(angle)`、`rotateZ(angle)`
- `rotateX(s, c)`、`rotateY(s, c)`、`rotateZ(s, c)`

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
- `normalizeFast()`
- 四元数乘法 `operator*`
- `rotate(const float3& v)`
- `inverse()`

欧拉角单位是弧度。`FromEuler` 使用 X 作为 pitch，Y 作为 yaw，Z 作为 roll。

`float4x4` 是初始化为单位矩阵的 4x4 矩阵：

```cpp
struct float4x4 {
    float m[16];
};
```

## 平台 API

平台层向引擎提供内存、文件、窗口、输入、时间和 framebuffer 提交。

像素布局：

```cpp
struct ColorA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};
```

文件数据：

```cpp
struct FileData {
    void *handle;
    const unsigned char *data;
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
- `Present(Window *window, int width, int height, const ColorA *pixels)`

`LoadFile` 返回的数据在调用 `CloseFile` 前有效。`CreateWindow` 返回的窗口必须用 `DestroyWindow` 释放。

## 引擎 API

`DirectionalLight` 控制方向光：

```cpp
struct DirectionalLight {
    float3 direction;
    float3 color;
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

`Texture` 拥有浮点 RGB 像素：

```cpp
class Texture {
public:
    int width;
    int height;
    float3 *pixels;
    bool valid;
};
```

Texture 函数：

- `float3 Sample(float u, float v) const`
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

    void Clear(float3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, float3 color);
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

XAPI 后端创建 XJ380 GUI 窗口，并通过 `xapi_WriteBufferA` 提交 `ColorA` 缓冲区。XJ380 键盘消息来自 `MSG_CHAR` 和 `MSG_SPCHAR`，键值读取自 `lData`。XJ380 发送按下消息，后端会在短超时后合成按键释放。

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
