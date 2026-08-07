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

static void AddTriangle(HE3D::float3 *vertices, HE3D::int32_t &count, HE3D::float3 a,
                        HE3D::float3 b, HE3D::float3 c)
{
   vertices[count++] = a;
   vertices[count++] = b;
   vertices[count++] = c;
}

static void AddRectangleFaces(HE3D::float3 *vertices, HE3D::int32_t &count, float minX, float minY,
                              float maxX, float maxY)
{
   HE3D::float3 a = {minX, minY, 0.5f}, b = {maxX, minY, 0.5f};
   HE3D::float3 c = {maxX, maxY, 0.5f}, d = {minX, maxY, 0.5f};
   AddTriangle(vertices, count, a, b, c);
   AddTriangle(vertices, count, a, c, d);
   a.z = b.z = c.z = d.z = -0.5f;
   AddTriangle(vertices, count, a, c, b);
   AddTriangle(vertices, count, a, d, c);
}

static HE3D::Mesh CreateConcaveU()
{
   HE3D::float3  vertices[162];
   HE3D::int32_t count = 0;
   AddRectangleFaces(vertices, count, -1.5f, -1.5f, -0.5f, -0.5f);
   AddRectangleFaces(vertices, count, -0.5f, -1.5f, 0.5f, -0.5f);
   AddRectangleFaces(vertices, count, 0.5f, -1.5f, 1.5f, -0.5f);
   AddRectangleFaces(vertices, count, -1.5f, -0.5f, -0.5f, 1.5f);
   AddRectangleFaces(vertices, count, 0.5f, -0.5f, 1.5f, 1.5f);
   const HE3D::float3 boundary[] = {
       {-1.5f, -1.5f, 0.0f}, {-0.5f, -1.5f, 0.0f}, {0.5f, -1.5f, 0.0f}, {1.5f, -1.5f, 0.0f},
       {1.5f, -0.5f, 0.0f},  {1.5f, 1.5f, 0.0f},   {0.5f, 1.5f, 0.0f},  {0.5f, -0.5f, 0.0f},
       {-0.5f, -0.5f, 0.0f}, {-0.5f, 1.5f, 0.0f},  {-1.5f, 1.5f, 0.0f}, {-1.5f, -0.5f, 0.0f}};
   for (HE3D::int32_t edge = 0; edge < 12; edge++) {
      HE3D::float3 a = boundary[edge], b = boundary[(edge + 1) % 12];
      a.z = b.z      = -0.5f;
      HE3D::float3 c = a, d = b;
      c.z = d.z = 0.5f;
      AddTriangle(vertices, count, a, b, d);
      AddTriangle(vertices, count, a, d, c);
   }
   return HE3D::Mesh::Create(vertices, count);
}

static HE3D::Mesh CreateConcaveCross()
{
   const HE3D::float3 boundary[] = {
       {-0.5f, -1.5f, 0.0f}, {0.5f, -1.5f, 0.0f}, {0.5f, -0.5f, 0.0f},  {1.5f, -0.5f, 0.0f},
       {1.5f, 0.5f, 0.0f},   {0.5f, 0.5f, 0.0f},  {0.5f, 1.5f, 0.0f},   {-0.5f, 1.5f, 0.0f},
       {-0.5f, 0.5f, 0.0f},  {-1.5f, 0.5f, 0.0f}, {-1.5f, -0.5f, 0.0f}, {-0.5f, -0.5f, 0.0f}};
   HE3D::float3  vertices[156];
   HE3D::int32_t count = 0;
   for (HE3D::int32_t edge = 0; edge < 12; edge++) {
      HE3D::float3 a = {0.0f, 0.0f, 0.5f}, b = boundary[edge], c = boundary[(edge + 1) % 12];
      b.z = c.z = 0.5f;
      AddTriangle(vertices, count, a, b, c);
      a.z = b.z = c.z = -0.5f;
      AddTriangle(vertices, count, a, c, b);
      a   = boundary[edge];
      b   = boundary[(edge + 1) % 12];
      a.z = b.z          = -0.5f;
      HE3D::float3 sideC = a, sideD = b;
      sideC.z = sideD.z = 0.5f;
      AddTriangle(vertices, count, a, b, sideD);
      AddTriangle(vertices, count, a, sideD, sideC);
   }
   return HE3D::Mesh::Create(vertices, count);
}

static void ConvexColliderModeRequiredConstruction()
{
   HE3D::Mesh           cubeMesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::ConvexCollider collider(cubeMesh, HE3D::ConvexBuildMode::SingleHull);
   Check(collider.IsValid(), "mode-required convex collider builds a valid cube");
   Check(collider.GetPartCount() == 1, "mode-required convex collider reports one part");
   HE3D::AABB bounds = collider.LocalAABB();
   Check(bounds.min.x == -0.5f && bounds.max.x == 0.5f,
         "mode-required convex collider exposes cube bounds");
}

static void ConvexColliderOwnershipAndRebuildFailure()
{
   HE3D::Mesh           cubeMesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::ConvexCollider collider(cubeMesh, HE3D::ConvexBuildMode::SingleHull);
   HE3D::AABB           before   = collider.LocalAABB();
   HE3D::Mesh           triangle = HE3D::Mesh::CreateTriangle(1.0f, 1.0f);
   Check(!collider.BuildFromMesh(triangle, HE3D::ConvexBuildMode::SingleHull),
         "invalid rebuild is rejected");
   Check(collider.IsValid() && collider.GetPartCount() == 1 &&
             collider.LocalAABB().min.x == before.min.x &&
             collider.LocalAABB().max.x == before.max.x,
         "failed rebuild preserves existing collider state");
}

static void ConvexDecompositionBuildsMultipartCaches()
{
   HE3D::Mesh           uMesh     = CreateConcaveU();
   HE3D::Mesh           crossMesh = CreateConcaveCross();
   HE3D::ConvexCollider uCollider(uMesh, HE3D::ConvexBuildMode::ConvexDecomposition);
   HE3D::ConvexCollider crossCollider(crossMesh, HE3D::ConvexBuildMode::ConvexDecomposition);
   Check(uCollider.IsValid() && uCollider.GetPartCount() > 1,
         "public decomposition builds multiple U collider parts");
   Check(crossCollider.IsValid() && crossCollider.GetPartCount() > 1,
         "public decomposition builds multiple Cross collider parts");
   Check(uCollider.LocalAABB().min.x < -1.4f && uCollider.LocalAABB().max.x > 1.4f &&
             crossCollider.LocalAABB().min.y < -1.4f && crossCollider.LocalAABB().max.y > 1.4f,
         "public decomposition exposes aggregate part bounds");
   HE3D::float3 inertia = uCollider.EstimateInertia(2.0f);
   Check(inertia.x > 0.0f && inertia.y > 0.0f && inertia.z > 0.0f,
         "public decomposition exposes aggregate positive inertia");

   HE3D::AABB    before  = uCollider.LocalAABB();
   HE3D::int32_t parts   = uCollider.GetPartCount();
   HE3D::Mesh    invalid = HE3D::Mesh::CreateTriangle();
   Check(!uCollider.BuildFromMesh(invalid, HE3D::ConvexBuildMode::ConvexDecomposition) &&
             uCollider.IsValid() && uCollider.GetPartCount() == parts &&
             uCollider.LocalAABB().min.x == before.min.x &&
             uCollider.LocalAABB().max.x == before.max.x,
         "failed decomposition rebuild preserves multipart cache atomically");
}

static void ConvexDecompositionOwnsBuildCacheAndRegistersBodies()
{
   HE3D::ConvexCollider collider;
   {
      HE3D::Mesh source = CreateConcaveU();
      Check(collider.BuildFromMesh(source, HE3D::ConvexBuildMode::ConvexDecomposition),
            "decomposition accepts a source mesh before its lifetime ends");
   }
   Check(collider.IsValid() && collider.GetPartCount() > 1,
         "decomposition collider retains its build cache after source mesh destruction");

   HE3D::Mesh              objectMesh = HE3D::Mesh::CreateCube();
   HE3D::GameObject        staticObject(objectMesh);
   HE3D::GameObject        kinematicObject(objectMesh);
   HE3D::GameObject        dynamicObject(objectMesh);
   HE3D::PhysicsMaterial   material;
   HE3D::PhysicsProperties properties;
   HE3D::PhysicsScene      scene(3);
   Check(scene.AddStaticBody(staticObject, collider, material),
         "multipart convex collider registers as static body");
   Check(scene.AddKinematicBody(kinematicObject, collider, material),
         "multipart convex collider registers as kinematic body");
   Check(scene.AddDynamicBody(dynamicObject, collider, properties),
         "multipart convex collider registers as dynamic body");
}

static void ConvexColliderInertiaAndRegistration()
{
   HE3D::Mesh           cubeMesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::ConvexCollider collider(cubeMesh, HE3D::ConvexBuildMode::SingleHull);
   HE3D::float3         inertia = collider.EstimateInertia(2.0f);
   Check(inertia.x > 0.0f && inertia.y > 0.0f && inertia.z > 0.0f,
         "single-hull convex inertia is finite and positive");

   HE3D::GameObject        cube(cubeMesh);
   HE3D::PhysicsMaterial   material;
   HE3D::PhysicsProperties properties;
   HE3D::PhysicsScene      scene(2);
   Check(scene.AddStaticBody(cube, collider, material), "convex collider registers as static body");
   Check(scene.RemoveBody(cube), "registered convex collider can be removed");
   Check(scene.AddDynamicBody(cube, collider, properties),
         "convex collider registers as dynamic body");
}

int main()
{
   ConvexColliderModeRequiredConstruction();
   ConvexColliderOwnershipAndRebuildFailure();
   ConvexDecompositionBuildsMultipartCaches();
   ConvexDecompositionOwnsBuildCacheAndRegistersBodies();
   ConvexColliderInertiaAndRegistration();
   if (g_failures == 0) {
      std::printf("he3d_convex_collider_tests passed\n");
   }
   return g_failures == 0 ? 0 : 1;
}
