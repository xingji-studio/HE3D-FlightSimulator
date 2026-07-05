#include "he3d.hpp"

static bool g_keys[256];
static bool g_quit = false;

static void OnKey(HE3D::int32_t key, bool pressed, void *)
{
    if (key >= 'A' && key <= 'Z')
    {
        key = key - 'A' + 'a';
    }
    if (key >= 0 && key < 256)
    {
        g_keys[key] = pressed;
    }
    if (key == 27 && pressed)
    {
        g_quit = true;
    }
}

static void ResetBody(HE3D::GameObject& cube, HE3D::PhysicsBody& body)
{
    cube.position = {0.0f, 3.0f, 4.0f};
    cube.orientation = HE3D::quat();
    body.SetVelocity({1.0f, 0.0f, 0.0f});
    body.SetAngularVelocity({0.0f, 0.0f, 0.0f});
    body.ClearForces();
    body.ClearTorques();
}

int main()
{
    HE3D::WindowDesc desc;
    desc.width = 640;
    desc.height = 360;
    desc.title = "HE3D PhysicsTest";
    desc.flags = 0;

    HE3D::Window *window = HE3D::CreateWindow(&desc);
    if (!window)
    {
        return 1;
    }

    HE3D::SetKeyCallback(window, OnKey, nullptr);
    HE3D::SetFrameRateLimit(60);
    HE3D::SetFxaaEnabled(false);

    HE3D::Renderer renderer(window, desc.width, desc.height);

    HE3D::DirectionalLight light;
    light.direction = {0.4f, 0.8f, -1.0f};
    light.color = {1.0f, 1.0f, 1.0f};
    light.ambient = 0.25f;
    renderer.SetMainLight(light);

    HE3D::Camera camera;
    camera.position = {0.0f, 1.4f, -7.0f};
    camera.orientation = HE3D::quat();
    camera.fov = 70.0f;

    HE3D::GameObject floor;
    floor.mesh = HE3D::Mesh::CreateCube(7.0f, 0.2f, 7.0f);
    floor.position = {0.0f, -0.6f, 4.0f};

    HE3D::GameObject cube;
    cube.mesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);

    if (!floor.mesh || !cube.mesh)
    {
        delete floor.mesh;
        delete cube.mesh;
        HE3D::DestroyWindow(window);
        return 1;
    }

    HE3D::CollisionBox floorBox(&floor);
    HE3D::CollisionBox cubeBox(&cube);
    if (!floorBox.FitMesh() || !cubeBox.FitMesh())
    {
        delete floor.mesh;
        delete cube.mesh;
        HE3D::DestroyWindow(window);
        return 1;
    }

    HE3D::PhysicsBody body(&cube);
    body.SetEnabled(true);
    body.SetMass(1.0f);
    body.SetInertia(1.0f);
    body.friction = 0.35f;
    body.restitution = 0.15f;
    ResetBody(cube, body);

    bool fxaaWasDown = false;
    bool jumpWasDown = false;
    bool grounded = false;
    double lastTime = HE3D::TimeSeconds();

    while (!g_quit && !HE3D::WindowShouldClose(window))
    {
        double frameStart = HE3D::TimeSeconds();
        HE3D::PollEvents(window);

        double now = HE3D::TimeSeconds();
        float dt = (float)(now - lastTime);
        lastTime = now;
        if (dt > 0.05f)
        {
            dt = 0.05f;
        }

        bool fxaaDown = g_keys['f'];
        if (fxaaDown && !fxaaWasDown)
        {
            HE3D::SetFxaaEnabled(!HE3D::IsFxaaEnabled());
        }
        fxaaWasDown = fxaaDown;

        if (g_keys['r'])
        {
            ResetBody(cube, body);
            grounded = false;
        }

        if (g_keys['a'])
        {
            body.AddForce({-14.0f, 0.0f, 0.0f});
            body.AddTorque({0.0f, 0.0f, 4.0f});
        }
        if (g_keys['d'])
        {
            body.AddForce({14.0f, 0.0f, 0.0f});
            body.AddTorque({0.0f, 0.0f, -4.0f});
        }

        bool jumpDown = g_keys[' '];
        if (jumpDown && !jumpWasDown && grounded)
        {
            body.AddImpulse({0.0f, 5.0f, 0.0f});
            grounded = false;
        }
        jumpWasDown = jumpDown;

        HE3D::PhysicsContact contact;
        grounded = body.StepWithCollisions(dt, cubeBox, &floorBox, 1, 8, &contact);
        if (cube.position.y < -5.0f)
        {
            ResetBody(cube, body);
            grounded = false;
        }

        renderer.Clear({0.08f, 0.10f, 0.13f});
        renderer.DrawGameObject(floor, camera, HE3D::color3(0.25f, 0.55f, 0.35f));
        renderer.DrawGameObject(cube, camera, grounded ? HE3D::color3(0.95f, 0.75f, 0.25f)
                                                       : HE3D::color3(0.35f, 0.65f, 1.0f));
        renderer.Present();
        HE3D::PaceFrame(frameStart);
    }

    delete floor.mesh;
    delete cube.mesh;
    HE3D::DestroyWindow(window);
    return 0;
}
