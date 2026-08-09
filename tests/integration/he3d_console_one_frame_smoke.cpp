#include "he3d.hpp"

#include <cstdio>

int main()
{
   HE3D::WindowDesc description = {48, 32, "HE3D one-frame smoke", 0};
   HE3D::Window    *window      = HE3D::CreateWindow(&description);
   if (!window) {
      std::fprintf(stderr, "CreateWindow failed\n");
      return 1;
   }

   HE3D::SetFxaaEnabled(false);
   HE3D::SetTaaEnabled(false);
   HE3D::SetMsaaEnabled(false);
   HE3D::SetSsaaScale(1);

   HE3D::Mesh mesh = HE3D::Mesh::CreateTriangle(1.8f, 1.8f);
   if (!mesh.IsValid()) {
      std::fprintf(stderr, "triangle mesh creation failed\n");
      HE3D::DestroyWindow(window);
      return 2;
   }

   bool frameIsValid = false;
   {
      HE3D::GameObject object(mesh);
      object.position = {0.0f, 0.0f, 3.0f};
      HE3D::Camera camera;
      camera.position = {0.0f, 0.0f, 0.0f};
      camera.fov      = 70.0f;

      HE3D::Renderer renderer(window, description.width, description.height);
      object.color = {1.0f, 0.2f, 0.1f};
      renderer.SetCamera(camera);
      renderer.AddObject(object);
      renderer.RenderFrame({0.0f, 0.0f, 0.0f});

      const HE3D::ColorA *pixels          = renderer.GetPresentedPixels();
      const bool          dimensionsMatch = renderer.GetPresentedWidth() == description.width &&
                                            renderer.GetPresentedHeight() == description.height;
      if (pixels && dimensionsMatch) {
         const int pixelCount = description.width * description.height;
         for (int index = 0; index < pixelCount; ++index) {
            if (pixels[index].r != 0 || pixels[index].g != 0 || pixels[index].b != 0) {
               frameIsValid = true;
               break;
            }
         }
      }
   }

   HE3D::DestroyWindow(window);
   if (!frameIsValid) {
      std::fprintf(stderr, "Presented pixels did not contain rendered foreground\n");
      return 3;
   }
   std::printf("Presented pixels contain non-background output\n");
   return 0;
}
