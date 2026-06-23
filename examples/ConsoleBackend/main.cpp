#include "he3d.hpp"

#include <chrono>
#include <thread>

int main()
{
    HE3D::WindowDesc desc = {};
    desc.width = 80;
    desc.height = 40;
    desc.title = "HE3D Console Backend";
    desc.flags = 0;

    HE3D::Window *window = HE3D::CreateWindow(&desc);
    if (!window)
    {
        return 1;
    }

    HE3D::Renderer renderer(window, desc.width, desc.height);

    const HE3D::float3 vertices[] = {
        {-1.2f, -0.8f, 4.0f},
        { 1.2f, -0.8f, 4.0f},
        { 0.0f,  1.1f, 4.0f},
    };
    const HE3D::float2 uvs[] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.5f, 1.0f},
    };

    HE3D::Mesh *mesh = HE3D::Mesh::Create(vertices, uvs, 3);
    if (!mesh)
    {
        HE3D::DestroyWindow(window);
        return 1;
    }

    HE3D::GameObject triangle;
    triangle.mesh = mesh;

    HE3D::Camera camera;
    camera.position = {0.0f, 0.0f, 0.0f};
    camera.fov = 70.0f;
    camera.targetFov = 70.0f;

    HE3D::DirectionalLight light;
    light.direction = {0.2f, 0.4f, 1.0f};
    light.color = {1.0f, 0.95f, 0.85f};
    light.ambient = 0.25f;
    renderer.SetMainLight(light);

    for (int frame = 0; frame < 180 && !HE3D::WindowShouldClose(window); frame++)
    {
        HE3D::PollEvents(window);

        float angle = (float)frame * 0.045f;
        triangle.orientation = HE3D::quat::FromEuler({angle * 0.45f, angle, angle * 0.25f});

        float pulse = 0.5f + 0.5f * HE3D::sinf(angle * 1.7f);
        HE3D::float3 color = {0.2f + pulse * 0.6f, 0.45f, 1.0f - pulse * 0.35f};

        renderer.Clear({0.02f, 0.025f, 0.035f});
        renderer.DrawGameObject(triangle, camera, color);
        renderer.Present();

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    delete mesh;
    HE3D::DestroyWindow(window);
    return 0;
}
