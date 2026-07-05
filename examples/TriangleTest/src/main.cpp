/*
 * HE3D TriangleTest - the smallest useful example.
 * HE3D TriangleTest：最小可用示例。
 *
 * This file shows the basic HE3D program shape:
 * create a window, create a mesh, draw every frame, then release resources.
 *
 * 本文件展示 HE3D 程序的基本形状：
 * 创建窗口，创建 mesh，每帧绘制，最后释放资源。
 */
#include "he3d.hpp"

static bool g_quit = false;

// HE3D sends keyboard events here after PollEvents() is called.
// 调用 PollEvents() 后，HE3D 会把键盘事件发送到这里。
static void OnKey(HE3D::int32_t key, bool pressed, void *)
{
    if (key == 27 && pressed)
    {
        g_quit = true;
    }
}

int main()
{
    // WindowDesc describes the window we want.
    // WindowDesc 描述我们要创建的窗口。
    HE3D::WindowDesc desc;
    desc.width = 640;
    desc.height = 360;
    desc.title = "HE3D TriangleTest";
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

    // Limit this beginner example to 60 FPS so it does not waste CPU.
    // 把这个入门示例限制到 60 FPS，避免浪费 CPU。
    HE3D::SetFrameRateLimit(60);

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

    // The default camera looks along +Z, so objects in front of it should have positive z.
    // 默认相机看向 +Z，所以放在相机前方的物体应该有正 z 坐标。
    HE3D::Camera camera;
    camera.position = {0.0f, 0.0f, 0.0f};
    camera.orientation = HE3D::quat();
    camera.fov = 70.0f;

    // GameObject combines a mesh with a position and rotation.
    // GameObject 把 mesh、位置和旋转组合成一个物体。
    HE3D::GameObject triangle;
    triangle.mesh = HE3D::Mesh::CreateTriangle(1.6f, 1.4f);
    triangle.position = {0.0f, 0.0f, 3.0f};

    // If mesh allocation failed, destroy the window before returning.
    // 如果 mesh 分配失败，返回前要先销毁窗口。
    if (!triangle.mesh)
    {
        HE3D::DestroyWindow(window);
        return 1;
    }

    // lastTime is used to compute deltaTime, so motion speed does not depend on FPS.
    // lastTime 用来计算 deltaTime，让运动速度不依赖 FPS。
    double lastTime = HE3D::TimeSeconds();
    float angle = 0.0f;

    bool triangleGoForward = true;

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

        // deltaTime is the time passed since the previous frame.
        // deltaTime 是距离上一帧过去的时间。
        double now = HE3D::TimeSeconds();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;

        // Clamp large pauses so the triangle does not jump after a debugger stop or window stall.
        // 限制过大的暂停时间，避免调试暂停或窗口卡顿后物体突然跳变。
        // if (deltaTime > 0.1f)
        // {
        //     deltaTime = 0.1f;
        // }

        if (triangleGoForward) {
            if (triangle.position.z < 3.0f) {
                triangle.position.z += deltaTime * 2.0f;
            } else {
                triangleGoForward = !triangleGoForward;
            }
        } else {
            if (triangle.position.z > 1.0f) {
                triangle.position.z -= deltaTime * 2.0f;
            } else {
                triangleGoForward = !triangleGoForward;
            }
        }

        // Rotate the triangle around local Z; FromEuler takes radians.
        // 让三角形绕局部 Z 轴旋转；FromEuler 使用弧度。
        angle += deltaTime;
        triangle.orientation = HE3D::quat::FromEuler({0.0f, 0.0f, angle});

        // Clear starts a new frame, DrawGameObject writes pixels, Present shows the result.
        // Clear 开始新一帧，DrawGameObject 写入像素，Present 显示结果。
        renderer.Clear({0.08f, 0.10f, 0.14f});
        renderer.DrawGameObject(triangle, camera, HE3D::color3(1.0f, 0.35f, 0.15f));
        renderer.Present();

        // If SetFrameRateLimit(60) was called, PaceFrame waits for the remaining frame time.
        // 如果调用过 SetFrameRateLimit(60)，PaceFrame 会等待本帧剩余时间。
        HE3D::PaceFrame(frameStart);
    }

    // Release resources created by this example.
    // 释放这个示例创建的资源。
    delete triangle.mesh;
    HE3D::DestroyWindow(window);
    return 0;
}
