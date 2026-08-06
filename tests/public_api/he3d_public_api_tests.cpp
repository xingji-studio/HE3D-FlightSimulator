#include "he3d.hpp"

#include <cstdio>

static int g_failures = 0;

static void Check(bool condition, const char *name)
{
   if (!condition) {
      std::printf("FAIL: %s\n", name);
      g_failures++;
   }
}

static bool Near(float a, float b, float tolerance)
{
   float d = a - b;
   if (d < 0.0f) d = -d;
   return d <= tolerance;
}

static void MeshValuesAndRaycastsWork()
{
   HE3D::Mesh triangle = HE3D::Mesh::CreateTriangle();
   Check(triangle.IsValid() && triangle.GetVertexCount() == 3,
         "Mesh::CreateTriangle returns a valid value");
   Check(triangle.GetVertices() && triangle.GetUVs() && triangle.GetTriangleNormals(),
         "Mesh exposes read-only geometry views");

   HE3D::GameObject object(triangle);
   object.position  = {0.0f, 0.0f, 2.0f};
   HE3D::RayHit hit = HE3D::RaycastMeshTriangles(HE3D::Ray({0, 0, 0}, {0, 0, 1}), object);
   Check(hit.hit && hit.triangleIndex == 0, "RaycastMeshTriangles returns hit triangle index");

   HE3D::Mesh invalid = HE3D::Mesh::CreateCube(1.0f, 0.0f, 1.0f);
   Check(!invalid.IsValid(), "Mesh factory returns invalid value on bad dimensions");
}

static void ObjLoaderCountsVertices()
{
   const char *path = "/tmp/he3d_public_api_triangle.obj";
   FILE       *file = std::fopen(path, "wb");
   Check(file != nullptr, "public API test can create temporary OBJ file");
   if (!file) return;

   const char obj[] = "v 0 0 0\n"
                      "v 1 0 0\n"
                      "v 0 1 0\n"
                      "vt 0 0\n"
                      "vt 1 0\n"
                      "vt 0 1\n"
                      "f 1/1 2/2 3/3\n";
   std::fwrite(obj, 1, sizeof(obj) - 1, file);
   std::fclose(file);

   HE3D::Mesh mesh = HE3D::Mesh::LoadOBJ(path);
   Check(mesh.IsValid() && mesh.GetVertexCount() == 3, "Mesh::LoadOBJ parses vertex count itself");
   Check(mesh.IsValid() && Near(mesh.GetVertices()[1].x, 1.0f, 0.0001f),
         "OBJ loader preserves vertex positions");
   std::remove(path);
}

static void TextureCreateCopiesPixelsAndLoadImageUsesHeaders()
{
   HE3D::ColorA pixels[4] = {
       {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {255, 255, 255, 255}};
   HE3D::Texture texture = HE3D::Texture::Create(pixels, 2, 2);
   pixels[2]             = {0, 0, 0, 255};
   HE3D::color3 wrapped  = texture.Sample(1.25f, -0.25f);
   Check(texture.IsValid() && texture.GetWidth() == 2 && texture.GetHeight() == 2,
         "Texture::Create reports dimensions and validity");
   Check(Near(wrapped.b, 1.0f, 0.0001f), "Texture::Create copies caller-owned pixels");
   Check(!HE3D::Texture::Create(nullptr, 1, 1).IsValid() &&
             !HE3D::Texture::Create(pixels, 0, 1).IsValid() &&
             !HE3D::Texture::Create(pixels, 8193, 1).IsValid(),
         "Texture::Create rejects invalid input");

   const char *bmp  = "/tmp/he3d_header_detect_texture.notbmp";
   FILE       *file = std::fopen(bmp, "wb");
   Check(file != nullptr, "public API test can create temporary BMP file");
   if (file) {
      unsigned char data[70] = {};
      data[0]                = 'B';
      data[1]                = 'M';
      data[2]                = 70;
      data[10]               = 54;
      data[14]               = 40;
      data[18]               = 2;
      data[22]               = 2;
      data[26]               = 1;
      data[28]               = 24;
      data[54]               = 255;
      data[55]               = 0;
      data[56]               = 0;
      data[57]               = 0;
      data[58]               = 255;
      data[59]               = 0;
      data[62]               = 0;
      data[63]               = 0;
      data[64]               = 255;
      data[65]               = 255;
      data[66]               = 255;
      data[67]               = 255;
      std::fwrite(data, 1, sizeof(data), file);
      std::fclose(file);
      HE3D::Texture loaded = HE3D::Texture::LoadImage(bmp);
      Check(loaded.IsValid() && loaded.GetWidth() == 2 && loaded.GetHeight() == 2,
            "Texture::LoadImage detects BMP content despite extension");
   }
   std::remove(bmp);

   const char *empty = "/tmp/he3d_empty_texture";
   file              = std::fopen(empty, "wb");
   if (file) std::fclose(file);
   Check(!HE3D::Texture::LoadImage(empty).IsValid(), "Texture::LoadImage rejects empty files");
   std::remove(empty);
}

static void ColliderAndPhysicsSceneUseNewApi()
{
   HE3D::Mesh cubeMesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::Mesh floorMesh = HE3D::Mesh::CreatePlane(4.0f, 4.0f);
   HE3D::GameObject cube(cubeMesh);
   HE3D::GameObject floor(floorMesh);
   HE3D::BoxCollider box(1.0f, 1.0f, 1.0f);
   HE3D::BoxCollider invalidBox(0.0f, 1.0f, 1.0f);
   HE3D::MeshCollider floorCollider(floorMesh);
   HE3D::ConvexCollider convex(cubeMesh);
   HE3D::PhysicsMaterial floorMaterial;
   HE3D::PhysicsProperties properties;
   Check(box.IsValid() && box.GetKind() == HE3D::ColliderKind::Box,
         "BoxCollider exposes the common Collider contract");
   Check(!invalidBox.IsValid() && !invalidBox.SetSize(-1.0f, 1.0f, 1.0f),
         "BoxCollider rejects invalid dimensions");
   Check(convex.IsValid() && convex.GetKind() == HE3D::ColliderKind::Convex,
         "ConvexCollider builds a valid collider");
   Check(floorCollider.IsValid() && floorCollider.GetKind() == HE3D::ColliderKind::Mesh,
         "MeshCollider owns a valid borrowed-mesh view");
   floorMesh = HE3D::Mesh::CreatePlane(5.0f, 5.0f);
   floorCollider.Refresh();
   Check(floorCollider.IsValid(), "MeshCollider refreshes after its borrowed mesh lifecycle changes");
   Check(properties.SetMass(2.0f) == HE3D::PhysicsSettingResult::Applied &&
             properties.SetMass(0.0f) == HE3D::PhysicsSettingResult::Rejected &&
             properties.SetInertia(box.EstimateInertia(2.0f)) == HE3D::PhysicsSettingResult::Applied,
         "PhysicsProperties validates body values");
   Check(floorMaterial.SetFriction(-1.0f) == HE3D::PhysicsSettingResult::Rejected &&
             floorMaterial.SetRestitution(1.5f) == HE3D::PhysicsSettingResult::AppliedWithWarning,
         "PhysicsMaterial validates contact values");

   HE3D::PhysicsScene scene(3);
   HE3D::PhysicsScene invalidScene(0);
   Check(scene.IsValid() && !invalidScene.IsValid(),
         "PhysicsScene reports registration storage validity");
   Check(scene.SetGravity({0.0f, 0.0f, 0.0f}) == HE3D::PhysicsSettingResult::Applied &&
             scene.SetDrag(0.0f) == HE3D::PhysicsSettingResult::Applied,
         "PhysicsScene accepts finite gravity and drag");
   Check(scene.AddDynamicBody(cube, box, properties), "PhysicsScene registers dynamic body");
   Check(scene.AddStaticBody(floor, floorCollider, floorMaterial),
         "PhysicsScene registers static body");
   Check(scene.HasBody(cube) && scene.HasBody(floor), "PhysicsScene tracks registered bodies");
   Check(scene.SetVelocity(cube, {1.0f, 0.0f, 0.0f}) &&
             !scene.SetVelocity(cube, {0.0f / 0.0f, 0.0f, 0.0f}) &&
             scene.AddForce(cube, {1.0f, 0.0f, 0.0f}) &&
             !scene.AddForce(cube, {0.0f / 0.0f, 0.0f, 0.0f}),
         "PhysicsScene rejects nonfinite runtime inputs");
   Check(!scene.AddDynamicBody(floor, floorCollider, properties),
         "PhysicsScene rejects unsupported dynamic mesh registration");
   HE3D::PhysicsContact contacts[2];
   Check(scene.GetContacts(cube, contacts, 2) == 0,
         "PhysicsScene contact query does not compute contacts in registration slice");
   Check(scene.RemoveBody(cube) && !scene.HasBody(cube) && !scene.RemoveBody(cube),
         "PhysicsScene removes registered body exactly once");
   scene.Clear();
   Check(!scene.HasBody(floor), "PhysicsScene clears registration storage");
   Check(!invalidScene.AddDynamicBody(cube, box, properties),
         "PhysicsScene rejects registration on invalid storage");
}

int main()
{
   MeshValuesAndRaycastsWork();
   ObjLoaderCountsVertices();
   TextureCreateCopiesPixelsAndLoadImageUsesHeaders();
   ColliderAndPhysicsSceneUseNewApi();

   if (g_failures == 0) {
      std::printf("he3d_public_api_tests passed\n");
   }
   return g_failures == 0 ? 0 : 1;
}
