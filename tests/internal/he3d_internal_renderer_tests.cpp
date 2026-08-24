#define private public
#include "he3d.hpp"
#undef private

#include <cstdio>

static int g_failures = 0;

static void Check(bool condition, const char *name)
{
    if (!condition) {
       std::printf("FAIL: %s\n", name);
       g_failures++;
    }
}

static bool HasForeground(const HE3D::ColorA *pixels, int pixelCount)
{
    for (int index = 0; pixels && index < pixelCount; ++index) {
       if (pixels[index].r != 0 || pixels[index].g != 0 || pixels[index].b != 0) {
          return true;
       }
    }
    return false;
}

static void RegisteredObjectsHaveStableFrameStorage()
{
    HE3D::SetFxaaEnabled(false);
    HE3D::SetTaaEnabled(false);
    HE3D::SetMsaaEnabled(false);
    HE3D::SetSsaaScale(1);

    HE3D::Mesh       mesh = HE3D::Mesh::CreateTriangle(2.0f, 2.0f);
    HE3D::GameObject first(mesh);
    HE3D::GameObject second(mesh);
    HE3D::Camera     camera;
    HE3D::Renderer   renderer(nullptr, 32, 32);
    first.position  = {0.0f, 0.0f, 3.0f};
    second.position = {0.0f, 0.0f, 4.0f};

    renderer.SetCamera(camera);
    Check(renderer.AddObject(first) && renderer.AddObject(second),
          "internal renderer test registers objects");
    HE3D::GameObject **objects = renderer.m_sceneObjects;
    const int capacity = renderer.m_sceneObjectCapacity;
    renderer.RenderFrame({0.0f, 0.0f, 0.0f});
    Check(renderer.m_sceneObjects == objects && renderer.m_sceneObjectCapacity == capacity,
          "RenderFrame does not allocate registered object storage");
    Check(renderer.m_sceneObjects[0] == &first && renderer.m_sceneObjects[1] == &second,
          "registered object order is deterministic");

    first.visible = false;
    second.visible = false;
    renderer.RenderFrame({0.0f, 0.0f, 0.0f});
    Check(!HasForeground(renderer.GetPresentedPixels(), 32 * 32),
          "invisible objects produce no foreground output");

    Check(renderer.RemoveObject(first) && renderer.m_sceneObjects[0] == &second,
          "removing an object preserves remaining registration order");
}

static void FailedConstructionLeavesBuffersNull()
{
    HE3D::SetSsaaScale(1);
    HE3D::Renderer oversized(nullptr, 50000, 50000);
    Check(oversized.m_colorBuf == nullptr && oversized.m_depthBuf == nullptr &&
              oversized.m_fxaaBuf == nullptr && oversized.m_taaBuf == nullptr &&
              oversized.m_taaHistory == nullptr && oversized.m_taaDepth == nullptr &&
              oversized.m_ssaaBuf == nullptr && oversized.m_msaaColorBuf == nullptr &&
              oversized.m_msaaDepthBuf == nullptr,
          "failed renderer construction leaves every buffer null");
}

int main()
{
    RegisteredObjectsHaveStableFrameStorage();
    FailedConstructionLeavesBuffersNull();
    if (g_failures == 0) {
       std::printf("he3d_internal_renderer_tests passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
