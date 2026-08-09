#include "he3d.hpp"

static bool g_quit = false;

static void OnKey(HE3D::int32_t key, bool pressed, void *)
{
   if (key == 27 && pressed) {
      g_quit = true;
   }
}

int main()
{
   HE3D::WindowDesc desc;
   desc.width  = 640;
   desc.height = 360;
   desc.title  = "HE3D TriangleTest";
   desc.flags  = 0;

   HE3D::Window *window = HE3D::CreateWindow(&desc);
   if (!window) {
      return 1;
   }
   HE3D::SetKeyCallback(window, OnKey, nullptr);
   HE3D::SetFrameRateLimit(60);

   HE3D::Renderer renderer(window, desc.width, desc.height);
   HE3D::Camera   camera;
   camera.position = {0.0f, 0.0f, 0.0f};
   camera.fov      = 70.0f;

   HE3D::Mesh triangleMesh = HE3D::Mesh::CreateTriangle(1.6f, 1.4f);
   if (!triangleMesh.IsValid()) {
      HE3D::DestroyWindow(window);
      return 1;
   }
   HE3D::GameObject triangle(triangleMesh);
   triangle.position = {0.0f, 0.0f, 3.0f};
   triangle.color    = {1.0f, 0.35f, 0.15f};
   renderer.SetCamera(camera);
   renderer.AddObject(triangle);

   double lastTime = HE3D::TimeSeconds();
   float  angle    = 0.0f;

   while (!g_quit && !HE3D::WindowShouldClose(window)) {
      double frameStart = HE3D::TimeSeconds();
      HE3D::PollEvents(window);

      double now       = HE3D::TimeSeconds();
      float  deltaTime = static_cast<float>(now - lastTime);
      lastTime         = now;
      if (deltaTime > 0.1f) deltaTime = 0.1f;

      angle += deltaTime;
      triangle.orientation = HE3D::quat::FromEuler({0.0f, 0.0f, angle});

      renderer.RenderFrame({0.08f, 0.10f, 0.14f});
      HE3D::PaceFrame(frameStart);
   }

   HE3D::DestroyWindow(window);
   return 0;
}
