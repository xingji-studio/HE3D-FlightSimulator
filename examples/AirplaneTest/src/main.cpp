#include "he3d.hpp"

static bool g_quit = false;
static const char *WINDOW_TITLE = "HE3D AirplaneTest";
static bool g_taaKeyDown = false;
static bool g_msaaKeyDown = false;
static bool g_ssaaResizeRequested = false;

// HE3D sends keyboard events here after PollEvents() is called.
// 调用 PollEvents() 后，HE3D 会把键盘事件发送到这里。
static void OnKey(HE3D::int32_t key, bool pressed, void *)
{
    if (key >= 'A' && key <= 'Z')
    {
        key = key - 'A' + 'a';
    }
    if (key == 27 && pressed)
    {
        g_quit = true;
    }
    if (key == 't')
    {
        if (pressed && !g_taaKeyDown)
        {
            HE3D::SetTaaEnabled(!HE3D::IsTaaEnabled());
        }
        g_taaKeyDown = pressed;
    }
    if (key == 'm')
    {
        if (pressed && !g_msaaKeyDown)
        {
            HE3D::SetMsaaEnabled(!HE3D::IsMsaaEnabled());
        }
        g_msaaKeyDown = pressed;
    }
    if (pressed && key >= '1' && key <= '4')
    {
        HE3D::SetSsaaScale((HE3D::uint32_t)(key - '0'));
        g_ssaaResizeRequested = true;
    }
}

// Append an unsigned integer to a fixed-size C string buffer.
// 将无符号整数追加到固定长度 C 字符串缓冲区。
static void AppendUnsigned(char *dst, HE3D::int32_t *pos, HE3D::int32_t maxLen, HE3D::uint32_t value)
{
    char tmp[16];
    int n = 0;
    if (value == 0) {
        tmp[n++] = '0';
    } else {
        while (value > 0 && n < (int)sizeof(tmp)) {
            tmp[n++] = (char)('0' + (value % 10U));
            value /= 10U;
        }
    }
    while (n > 0 && *pos < maxLen - 1) {
        dst[(*pos)++] = tmp[--n];
    }
}

// Build the window title with the averaged FPS value.
// 构造带平均 FPS 的窗口标题。
static void BuildFpsTitle(char *dst, HE3D::int32_t maxLen, HE3D::uint32_t fps)
{
    int pos = 0;
    const char *prefix = WINDOW_TITLE;
    while (*prefix && pos < maxLen - 1) {
        dst[pos++] = *prefix++;
    }
    const char *mid = " - FPS ";
    while (*mid && pos < maxLen - 1) {
        dst[pos++] = *mid++;
    }
    AppendUnsigned(dst, &pos, maxLen, fps);
    const char *taa = HE3D::IsTaaEnabled() ? " - TAA on" : " - TAA off";
    while (*taa && pos < maxLen - 1) {
        dst[pos++] = *taa++;
    }
    const char *msaa = HE3D::IsMsaaEnabled() ? " - MSAA on" : " - MSAA off";
    while (*msaa && pos < maxLen - 1) {
        dst[pos++] = *msaa++;
    }
    const char *ssaa = " - SSAA ";
    while (*ssaa && pos < maxLen - 1) {
        dst[pos++] = *ssaa++;
    }
    AppendUnsigned(dst, &pos, maxLen, HE3D::GetSsaaScale());
    if (pos < maxLen - 1) {
        dst[pos++] = 'x';
    }
    dst[pos] = 0;
}

static void UpdateFpsTitle(HE3D::Window *window, HE3D::uint32_t fps)
{
    char title[96];
    BuildFpsTitle(title, (int)sizeof(title), fps);
    HE3D::SetWindowTitle(window, title);
}

int main()
{
    // WindowDesc describes the window we want.
    // WindowDesc 描述我们要创建的窗口。
    HE3D::WindowDesc desc;
    desc.width = 640;
    desc.height = 360;
    desc.title = WINDOW_TITLE;
    desc.flags = 0;

    // CreateWindow creates the window used by Renderer and input.
    // CreateWindow 创建 Renderer 和输入系统要使用的窗口。
    HE3D::Window *window = HE3D::CreateWindow(&desc);
    if (!window)
    {
        return 1;
    }

    // PollEvents() will call OnKey later; nullptr means we do not pass custom user data.
    // 后面调用 PollEvents() 时会触发 OnKey；nullptr 表示不传自定义用户数据。
    HE3D::SetKeyCallback(window, OnKey, nullptr);

    // Limit this asset-loading example to 60 FPS so it does not waste CPU.
    // 把这个资源加载示例限制到 60 FPS，避免浪费 CPU。
    HE3D::SetFrameRateLimit(60);
    HE3D::SetFxaaEnabled(false);
    HE3D::SetTaaEnabled(false);
    HE3D::SetMsaaEnabled(false);
    HE3D::SetSsaaScale(2);

    // Renderer draws the scene into the window.
    // Renderer 把场景绘制到窗口里。
    HE3D::Renderer renderer(window, desc.width, desc.height);

    // DirectionalLight is a simple sunlight-like light source.
    // DirectionalLight 是类似日光的简单方向光。
    HE3D::DirectionalLight sun;
    sun.direction = {0.3f, 0.6f, -1.0f};
    sun.color = {1.0f, 1.0f, 1.0f};
    sun.ambient = 0.2f;
    renderer.SetMainLight(sun);

    // The model is large and centered near the origin, so place the camera far back.
    // 模型尺寸较大且中心接近原点，所以把相机放远。
    HE3D::Camera camera;
    camera.position = {0.0f, 2.0f, -95.0f};
    camera.orientation = HE3D::quat();
    camera.fov = 70.0f;

    // GameObject combines a mesh with a position and rotation.
    // GameObject 把 mesh、位置和旋转组合成一个物体。
    HE3D::GameObject model;
    model.mesh = HE3D::Mesh::LoadOBJ("model.obj");
    if (!model.mesh)
    {
        HE3D::DestroyWindow(window);
        return 1;
    }
    model.position = {0.0f, -2.0f, 0.0f};

    // lastTime is used to compute deltaTime, so motion speed does not depend on FPS.
    // lastTime 用来计算 deltaTime，让运动速度不依赖 FPS。
    double lastTime = HE3D::TimeSeconds();
    double fpsStart = lastTime;
    HE3D::uint32_t fpsFrames = 0;
    float angle = 0.0f;

    bool airplaneGoForward = true;

    // Main loop: handle input, update state, draw one frame, then pace the frame.
    // 主循环：处理输入，更新状态，绘制一帧，然后按帧率限制等待。
    while (!g_quit && !HE3D::WindowShouldClose(window))
    {
        // frameStart is passed to PaceFrame() so frame limiting measures the whole frame.
        // frameStart 会传给 PaceFrame()，这样限帧统计的是整帧耗时。
        double frameStart = HE3D::TimeSeconds();

        // PollEvents must be called every frame; otherwise keyboard and close events will not update.
        // 每帧必须调用 PollEvents；否则键盘和关闭窗口事件不会更新。
        HE3D::PollEvents(window);
        if (g_ssaaResizeRequested)
        {
            renderer.Resize(desc.width, desc.height);
            g_ssaaResizeRequested = false;
        }

        // deltaTime is the time passed since the previous frame.
        // deltaTime 是距离上一帧过去的时间。
        double now = HE3D::TimeSeconds();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;
        fpsFrames++;

        double fpsElapsed = now - fpsStart;
        if (fpsElapsed >= 1.0)
        {
            HE3D::uint32_t fps = (HE3D::uint32_t)((double)fpsFrames / fpsElapsed + 0.5);
            UpdateFpsTitle(window, fps);
            fpsStart = now;
            fpsFrames = 0;
        }

        if (deltaTime > 0.1f)
        {
            deltaTime = 0.1f;
        }

        if (airplaneGoForward) {
            if (model.position.z < 80.0f) {
                model.position.z += deltaTime * 20.0f;
            } else {
                airplaneGoForward = !airplaneGoForward;
            }
        } else {
            if (model.position.z > -80.0f) {
                model.position.z -= deltaTime * 20.0f;
            } else {
                airplaneGoForward = !airplaneGoForward;
            }
        }

        // Rotate the airplane slowly so the loaded OBJ can be inspected.
        // 缓慢旋转飞机，方便观察加载出的 OBJ。
        angle += deltaTime;
        model.orientation = HE3D::quat::FromEuler({0.0f, angle * 0.35f, 0.0f});

        // Clear starts a new frame, DrawGameObject writes pixels, Present shows the result.
        // Clear 开始新一帧，DrawGameObject 写入像素，Present 显示结果。
        renderer.Clear({0.08f, 0.10f, 0.14f});
        renderer.DrawGameObject(model, camera, HE3D::color3(1.0f, 1.0f, 1.0f));
        renderer.Present();

        // If SetFrameRateLimit(60) was called, PaceFrame waits for the remaining frame time.
        // 如果调用过 SetFrameRateLimit(60)，PaceFrame 会等待本帧剩余时间。
        HE3D::PaceFrame(frameStart);
    }

    // Release resources created by this example.
    // 释放这个示例创建的资源。
    delete model.mesh;
    HE3D::DestroyWindow(window);
    return 0;
}
