#include "he3d.hpp"

static bool g_quit                = false;
static bool g_taaKeyDown          = false;
static bool g_msaaKeyDown         = false;
static bool g_ssaaResizeRequested = false;

static void OnKey(HE3D::int32_t key, bool pressed, void *)
{
   if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';
   if (key == 27 && pressed) g_quit = true;
   if (key == 't') {
      if (pressed && !g_taaKeyDown) HE3D::SetTaaEnabled(!HE3D::IsTaaEnabled());
      g_taaKeyDown = pressed;
   }
   if (key == 'm') {
      if (pressed && !g_msaaKeyDown) HE3D::SetMsaaEnabled(!HE3D::IsMsaaEnabled());
      g_msaaKeyDown = pressed;
   }
   if (pressed && key >= '1' && key <= '4') {
      HE3D::SetSsaaScale(static_cast<HE3D::uint32_t>(key - '0'));
      g_ssaaResizeRequested = true;
   }
}

int main()
{
   HE3D::WindowDesc desc;
   desc.width  = 640;
   desc.height = 360;
   desc.title  = "HE3D AirplaneTest";
   desc.flags  = 0;

   HE3D::Window *window = HE3D::CreateWindow(&desc);
   if (!window) return 1;
   HE3D::SetKeyCallback(window, OnKey, nullptr);
   HE3D::SetFrameRateLimit(60);
   HE3D::SetTaaEnabled(false);
   HE3D::SetMsaaEnabled(false);
   HE3D::SetSsaaScale(2);

   HE3D::Renderer renderer(window, desc.width, desc.height);
   HE3D::Camera   camera;
   camera.position = {0.0f, 2.0f, -95.0f};
   camera.fov      = 70.0f;

   HE3D::Mesh modelMesh = HE3D::Mesh::LoadOBJ("model.obj");
   if (!modelMesh.IsValid()) {
      HE3D::DestroyWindow(window);
      return 1;
   }
   HE3D::GameObject model(modelMesh);
   model.position = {0.0f, -2.0f, 0.0f};
   model.color = HE3D::color3(1.0f, 1.0f, 1.0f);
   const HE3D::GameObject *sceneObjects[] = {&model};

   double lastTime = HE3D::TimeSeconds();
   float  angle    = 0.0f;

   while (!g_quit && !HE3D::WindowShouldClose(window)) {
      double frameStart = HE3D::TimeSeconds();
      HE3D::PollEvents(window);
      if (g_ssaaResizeRequested) {
         renderer.Resize(desc.width, desc.height);
         g_ssaaResizeRequested = false;
      }

      double now       = HE3D::TimeSeconds();
      float  deltaTime = static_cast<float>(now - lastTime);
      lastTime         = now;
      if (deltaTime > 0.1f) deltaTime = 0.1f;

      angle += deltaTime;
      model.orientation = HE3D::quat::FromEuler({0.0f, angle * 0.35f, 0.0f});

      renderer.SetScene(camera, sceneObjects, 1);
      renderer.RenderFrame({0.08f, 0.10f, 0.14f});
      HE3D::PaceFrame(frameStart);
   }

   HE3D::DestroyWindow(window);
   return 0;
}
